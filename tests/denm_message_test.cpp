#include "denm_message.hpp"
#include <chrono>
#include <ctime>
#include <gtest/gtest.h>

class DenmMessageTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Set up a basic DENM message for testing
		denm.setStationId(1234567);
		denm.setActionId(20);

		// Set current time
		timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		denm.setDetectionTime(timestamp);
		denm.setReferenceTime(timestamp);

		// Set position
		denm.setEventPosition(57.779017, 12.774981, 190.0);

		// Set other fields
		denm.setRelevanceDistance(RelevanceDistance_lessThan50m);
		denm.setRelevanceTrafficDirection(RelevanceTrafficDirection_allTrafficDirections);
		denm.setValidityDuration(std::chrono::seconds(600));
		denm.setStationType(3);
		denm.setInformationQuality(0);
		denm.setCauseCode(CauseCodeType_accident);
		denm.setSubCauseCode(0);
	}

	DenmMessage denm;
	time_t timestamp;
};

TEST_F(DenmMessageTest, BasicGettersAndSetters) {
	EXPECT_EQ(denm.denm->header.stationID, 1234567);
	EXPECT_EQ(denm.denm->denm.management.actionID.sequenceNumber, 20);
	EXPECT_EQ(denm.denm->denm.management.stationType, 3);
	EXPECT_EQ(*denm.denm->denm.management.relevanceDistance, RelevanceDistance_lessThan50m);
	EXPECT_EQ(*denm.denm->denm.management.relevanceTrafficDirection, RelevanceTrafficDirection_allTrafficDirections);
	EXPECT_EQ(*denm.denm->denm.management.validityDuration, 600);
	EXPECT_EQ(denm.denm->denm.situation->informationQuality, 0);
	EXPECT_EQ(denm.denm->denm.situation->eventType.causeCode, CauseCodeType_accident);
	EXPECT_EQ(denm.denm->denm.situation->eventType.subCauseCode, 0);
}

TEST_F(DenmMessageTest, EventPosition) {
	const auto& pos = denm.denm->denm.management.eventPosition;
	// Note: positions are stored internally in 1/10 microdegrees
	EXPECT_NEAR(pos.latitude / 10000000.0, 57.779017, 0.000001);
	EXPECT_NEAR(pos.longitude / 10000000.0, 12.774981, 0.000001);
	EXPECT_NEAR(pos.altitude.altitudeValue / 100.0, 190.0, 0.01);
}

TEST_F(DenmMessageTest, TimestampFormatting) {
	// Set current time
	time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	denm.setDetectionTime(now);
	denm.setReferenceTime(now);

	// Access the underlying values
	long detectionValue, referenceValue;
	asn_INTEGER2long(&denm.denm->denm.management.detectionTime, &detectionValue);
	asn_INTEGER2long(&denm.denm->denm.management.referenceTime, &referenceValue);

	// Verify timestamps are after 2004 (ITS epoch)
	EXPECT_GT(detectionValue, 0L);
	EXPECT_GT(referenceValue, 0L);

	// Verify timestamps match each other
	EXPECT_EQ(detectionValue, referenceValue);

	// Verify timestamps are within expected range
	long expected_msec = (now - DenmMessage::UTC_2004) * 1000;
	EXPECT_EQ(detectionValue, expected_msec);
}

TEST_F(DenmMessageTest, EncodingAndDecoding) {
	// Build the DENM message

	// Create a new DenmMessage and decode the data

	auto encodedData = denm.getUperEncoded();
	DenmMessage decodedDenm;
	decodedDenm.fromUper(encodedData);

	// Verify the decoded message matches the original
	EXPECT_EQ(decodedDenm.denm->header.stationID, denm.denm->header.stationID);
	EXPECT_EQ(decodedDenm.denm->denm.management.actionID.sequenceNumber,
			  denm.denm->denm.management.actionID.sequenceNumber);
	EXPECT_EQ(decodedDenm.denm->denm.management.stationType, denm.denm->denm.management.stationType);

	// Verify position
	const auto& originalPos = denm.denm->denm.management.eventPosition;
	const auto& decodedPos	= decodedDenm.denm->denm.management.eventPosition;
	EXPECT_EQ(originalPos.latitude, decodedPos.latitude);
	EXPECT_EQ(originalPos.longitude, decodedPos.longitude);
	EXPECT_EQ(originalPos.altitude.altitudeValue, decodedPos.altitude.altitudeValue);

	// Verify timestamps
	long originalDetectionTime, decodedDetectionTime;
	asn_INTEGER2long(&denm.denm->denm.management.detectionTime, &originalDetectionTime);
	asn_INTEGER2long(&decodedDenm.denm->denm.management.detectionTime, &decodedDetectionTime);
	EXPECT_EQ(originalDetectionTime, decodedDetectionTime);
}

TEST_F(DenmMessageTest, InvalidTimestampHandling) {
	// Test timestamp before 2004 (ITS epoch)
	time_t invalid_time = 1072915199; // Dec 31, 2003 23:59:59 UTC
	EXPECT_THROW(denm.setDetectionTime(invalid_time), std::runtime_error);
	EXPECT_THROW(denm.setReferenceTime(invalid_time), std::runtime_error);
}

TEST_F(DenmMessageTest, InvalidEncodedDataHandling) {
	// Test decoding invalid data
	std::vector<unsigned char> invalid_data = {0x00, 0x01, 0x02, 0x03};
	EXPECT_THROW(denm.fromUper(invalid_data), std::runtime_error);
}

TEST_F(DenmMessageTest, SpeedAndHeading) {
	// Initialize location container if not present
	if (!denm.denm->denm.location) {
		denm.denm->denm.location = vanetza::asn1::allocate<LocationContainer_t>();
	}

	// Test speed
	if (!denm.denm->denm.location->eventSpeed) {
		denm.denm->denm.location->eventSpeed = vanetza::asn1::allocate<Speed>();
	}
	double speed									 = 13.89; // 50 km/h in m/s
	denm.denm->denm.location->eventSpeed->speedValue = static_cast<SpeedValue_t>(speed * 100);
	EXPECT_NEAR(denm.denm->denm.location->eventSpeed->speedValue / 100.0, speed, 0.01);

	// Test heading
	if (!denm.denm->denm.location->eventPositionHeading) {
		denm.denm->denm.location->eventPositionHeading = vanetza::asn1::allocate<Heading>();
	}
	double heading												 = 45.0; // degrees
	denm.denm->denm.location->eventPositionHeading->headingValue = static_cast<HeadingValue_t>(heading * 10);
	EXPECT_NEAR(denm.denm->denm.location->eventPositionHeading->headingValue / 10.0, heading, 0.1);

	// Test confidence values
	denm.denm->denm.location->eventSpeed->speedConfidence			  = 95;
	denm.denm->denm.location->eventPositionHeading->headingConfidence = 90;
	EXPECT_EQ(denm.denm->denm.location->eventSpeed->speedConfidence, 95);
	EXPECT_EQ(denm.denm->denm.location->eventPositionHeading->headingConfidence, 90);
}
