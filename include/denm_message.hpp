#ifndef DENM_MESSAGE_HPP
#define DENM_MESSAGE_HPP

#include "crow.h"
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vanetza/asn1/denm.hpp>
#include <vanetza/btp/data_request.hpp>
#include <vector>
class DenmMessage {
public:
	DenmMessage();
	~DenmMessage();
	DenmMessage(const DenmMessage& other);
	DenmMessage& operator=(const DenmMessage& other);

	// Core functionality
	std::vector<unsigned char> getUperEncoded() const;
	void fromUper(const std::vector<unsigned char>& data);
	nlohmann::json toJson() const;
	static DenmMessage fromJson(const nlohmann::json& j);

	// Direct access to DENM structure
	std::unique_ptr<DENM_t> denm = std::make_unique<DENM_t>();

	// Add setter methods
	void setStationId(uint32_t id) {
		denm->header.stationID = id;
	}
	void setActionId(uint32_t id) {
		denm->denm.management.actionID.sequenceNumber = id;
	}
	void setDetectionTime(time_t time) {
		denm->denm.management.detectionTime = createItsTimestamp(time);
	}
	void setReferenceTime(time_t time) {
		denm->denm.management.referenceTime = createItsTimestamp(time);
	}

	void setEventPosition(double latitude, double longitude, double altitude) {
		denm->denm.management.eventPosition.latitude			   = static_cast<int32_t>(latitude * 10000000);
		denm->denm.management.eventPosition.longitude			   = static_cast<int32_t>(longitude * 10000000);
		denm->denm.management.eventPosition.altitude.altitudeValue = static_cast<int32_t>(altitude * 100);
	}

	void setRelevanceDistance(RelevanceDistance_t distance) {
		if (!denm->denm.management.relevanceDistance) {
			denm->denm.management.relevanceDistance = vanetza::asn1::allocate<RelevanceDistance_t>();
		}
		*denm->denm.management.relevanceDistance = distance;
	}

	void setRelevanceTrafficDirection(RelevanceTrafficDirection_t direction) {
		if (!denm->denm.management.relevanceTrafficDirection) {
			denm->denm.management.relevanceTrafficDirection = vanetza::asn1::allocate<RelevanceTrafficDirection_t>();
		}
		*denm->denm.management.relevanceTrafficDirection = direction;
	}

	void setValidityDuration(std::chrono::seconds duration) {
		if (!denm->denm.management.validityDuration) {
			denm->denm.management.validityDuration = vanetza::asn1::allocate<ValidityDuration_t>();
		}
		*denm->denm.management.validityDuration = duration.count();
	}

	void setStationType(StationType_t type) {
		denm->denm.management.stationType = type;
	}

	void setInformationQuality(uint8_t quality) {
		if (!denm->denm.situation) {
			denm->denm.situation = vanetza::asn1::allocate<SituationContainer_t>();
		}
		denm->denm.situation->informationQuality = quality;
	}

	void setCauseCode(CauseCodeType_t code) {
		if (!denm->denm.situation) {
			denm->denm.situation = vanetza::asn1::allocate<SituationContainer_t>();
		}
		denm->denm.situation->eventType.causeCode = code;
	}

	void setSubCauseCode(uint8_t code) {
		if (!denm->denm.situation) {
			denm->denm.situation = vanetza::asn1::allocate<SituationContainer_t>();
		}
		denm->denm.situation->eventType.subCauseCode = code;
	}

	static const time_t UTC_2004 = 1072915200; // Jan 1, 2004 00:00:00 UTC

private:
	// Helper functions for timestamp handling
	static std::string formatItsTimestamp(const TimestampIts_t& timestamp);
	static TimestampIts_t createItsTimestamp(time_t unix_timestamp);
};

#endif // DENM_MESSAGE_HPP
