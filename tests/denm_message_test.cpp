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
	EXPECT_EQ(denm.getStationId(), 1234567);
	EXPECT_EQ(denm.getActionId(), 20);
	EXPECT_EQ(denm.getStationType(), 3);
	EXPECT_EQ(denm.getRelevanceDistance(), RelevanceDistance_lessThan50m);
	EXPECT_EQ(denm.getRelevanceTrafficDirection(), RelevanceTrafficDirection_allTrafficDirections);
	EXPECT_EQ(denm.getValidityDuration(), std::chrono::seconds(600));
	EXPECT_EQ(denm.getInformationQuality(), 0);
	EXPECT_EQ(denm.getCauseCode(), CauseCodeType_accident);
	EXPECT_EQ(denm.getSubCauseCode(), 0);
}

TEST_F(DenmMessageTest, EventPosition) {
	const auto& pos = denm.getEventPosition();
	// Note: positions are stored internally in 1/10 microdegrees
	EXPECT_NEAR(pos.latitude / 10000000.0, 57.779017, 0.000001);
	EXPECT_NEAR(pos.longitude / 10000000.0, 12.774981, 0.000001);
	EXPECT_NEAR(pos.altitude.altitudeValue / 100.0, 190.0, 0.01);
}

TEST_F(DenmMessageTest, TimestampFormatting) {
	std::string detectionTime = denm.getDetectionTimeFormatted();
	std::string referenceTime = denm.getReferenceTimeFormatted();

	// Verify the timestamp format (YYYY-MM-DD HH:MM:SS UTC)
	EXPECT_EQ(detectionTime.length(), 23);
	EXPECT_EQ(referenceTime.length(), 23);
	EXPECT_TRUE(detectionTime.find("UTC") != std::string::npos);
	EXPECT_TRUE(referenceTime.find("UTC") != std::string::npos);
}

TEST_F(DenmMessageTest, EncodingAndDecoding) {
	// Build and encode the DENM message
	auto denmMessage = denm.buildDenm();
	auto encodedDenm = denmMessage.encode();

	// Create a new DenmMessage and decode the data
	DenmMessage decodedDenm;
	decodedDenm.fromUper(std::vector<unsigned char>(encodedDenm.begin(), encodedDenm.end()));

	// Verify the decoded message matches the original
	EXPECT_EQ(decodedDenm.getStationId(), denm.getStationId());
	EXPECT_EQ(decodedDenm.getActionId(), denm.getActionId());
	EXPECT_EQ(decodedDenm.getStationType(), denm.getStationType());

	const auto& originalPos = denm.getEventPosition();
	const auto& decodedPos	= decodedDenm.getEventPosition();
	EXPECT_EQ(originalPos.latitude, decodedPos.latitude);
	EXPECT_EQ(originalPos.longitude, decodedPos.longitude);
	EXPECT_EQ(originalPos.altitude.altitudeValue, decodedPos.altitude.altitudeValue);

	EXPECT_EQ(decodedDenm.getRelevanceDistance(), denm.getRelevanceDistance());
	EXPECT_EQ(decodedDenm.getRelevanceTrafficDirection(), denm.getRelevanceTrafficDirection());
	EXPECT_EQ(decodedDenm.getValidityDuration(), denm.getValidityDuration());
	EXPECT_EQ(decodedDenm.getInformationQuality(), denm.getInformationQuality());
	EXPECT_EQ(decodedDenm.getCauseCode(), denm.getCauseCode());
	EXPECT_EQ(decodedDenm.getSubCauseCode(), denm.getSubCauseCode());
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
	// Test speed setting and getting
	double speed = 13.89; // 50 km/h in m/s
	denm.setEventSpeed(speed);
	EXPECT_NEAR(denm.getEventSpeed(), speed, 0.01);

	// Test heading setting and getting
	double heading = 45.0; // degrees
	denm.setEventHeading(heading);
	EXPECT_NEAR(denm.getEventHeading(), heading, 0.1);

	// Test confidence values
	denm.setEventSpeedConfidence(0.95);
	denm.setEventHeadingConfidence(0.90);
	EXPECT_NEAR(denm.getEventSpeedConfidence(), 0.95, 0.01);
	EXPECT_NEAR(denm.getEventHeadingConfidence(), 0.90, 0.01);
}
