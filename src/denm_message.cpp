#include "denm_message.hpp"
#include "crow.h"
#include <iomanip>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vanetza/units/angle.hpp>
#include <vanetza/units/velocity.hpp>

// Add private helper function declarations to the header first

// Add at the top with other constants
static const time_t UTC_2004 = 1072915200; // Jan 1, 2004 00:00:00 UTC

DenmMessage::DenmMessage() {
	// Initialize with default values
	denm->header.protocolVersion = 2;
	denm->header.messageID		 = ItsPduHeader__messageID_denm;
	denm->header.stationID		 = 0;

	// Initialize Management Container with defaults
	auto& mgmt						   = denm->denm.management;
	mgmt.actionID.originatingStationID = 0;
	mgmt.actionID.sequenceNumber	   = 0;
	mgmt.stationType				   = 0;

	// Initialize mandatory fields in management container
	mgmt.detectionTime = createItsTimestamp(std::time(nullptr));
	mgmt.referenceTime = createItsTimestamp(std::time(nullptr));

	// Initialize event position
	mgmt.eventPosition.latitude					   = 0;
	mgmt.eventPosition.longitude				   = 0;
	mgmt.eventPosition.altitude.altitudeValue	   = 0;
	mgmt.eventPosition.altitude.altitudeConfidence = AltitudeConfidence_unavailable;
}

DenmMessage::~DenmMessage() {
	if (denm) {
		ASN_STRUCT_FREE(asn_DEF_DENM, denm.release());
	}
}

DenmMessage::DenmMessage(const DenmMessage& other) :
  DenmMessage() {
	if (other.denm) {
		DENM_t* temp = nullptr;
		if (asn_DEF_DENM.op->copy_struct(&asn_DEF_DENM, reinterpret_cast<void**>(&temp), other.denm.get()) != 0) {
			throw std::runtime_error("Failed to copy DENM structure");
		}
		denm.reset(temp);
	}
}

DenmMessage& DenmMessage::operator=(const DenmMessage& other) {
	if (this != &other) {
		DenmMessage temp(other);	// Copy construct a temporary
		std::swap(denm, temp.denm); // Swap the contents
	}
	return *this;
}

// Helper function implementation
std::string DenmMessage::formatItsTimestamp(const TimestampIts_t& timestamp) {
	long msec_since_2004;
	if (asn_INTEGER2long(&timestamp, &msec_since_2004) != 0) {
		throw std::runtime_error("Failed to decode ITS timestamp");
	}

	if (msec_since_2004 < 0 || msec_since_2004 > 946080000000) {
		throw std::runtime_error("Invalid ITS timestamp value");
	}

	time_t unix_timestamp = UTC_2004 + (msec_since_2004 / 1000);
	std::stringstream ss;
	auto tm = std::gmtime(&unix_timestamp);
	if (!tm) {
		throw std::runtime_error("Failed to convert timestamp to UTC");
	}
	ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S UTC");
	return ss.str();
}

TimestampIts_t DenmMessage::createItsTimestamp(time_t unix_timestamp) {
	if (unix_timestamp < UTC_2004) {
		throw std::runtime_error("Timestamp before ITS epoch (2004-01-01)");
	}

	TimestampIts_t timestamp;
	// Initialize the buffer
	timestamp.buf = static_cast<uint8_t*>(calloc(1, sizeof(uint8_t)));
	if (!timestamp.buf) {
		throw std::runtime_error("Failed to allocate timestamp buffer");
	}
	timestamp.size = 1;

	int64_t msec_since_2004 = static_cast<int64_t>(unix_timestamp - UTC_2004) * 1000;
	if (asn_long2INTEGER(&timestamp, msec_since_2004) != 0) {
		free(timestamp.buf); // Clean up on error
		throw std::runtime_error("Failed to convert timestamp");
	}
	return timestamp;
}

void DenmMessage::fromUper(const std::vector<unsigned char>& data) {
	void* decoded = nullptr;
	asn_dec_rval_t rval;

	rval = uper_decode_complete(nullptr, &asn_DEF_DENM, &decoded, data.data(), data.size());

	if (rval.code != RC_OK) {
		ASN_STRUCT_FREE(asn_DEF_DENM, decoded);
		throw std::runtime_error("Failed to decode UPER data");
	}

	DENM_t* decoded_denm = static_cast<DENM_t*>(decoded);
	if (decoded_denm->header.messageID != ItsPduHeader__messageID_denm) {
		ASN_STRUCT_FREE(asn_DEF_DENM, decoded);
		throw std::runtime_error("Invalid message ID in decoded DENM");
	}

	// Use reset to handle the memory management
	denm.reset(decoded_denm);
}

// New method: Convert the DENM message to JSON.
nlohmann::json DenmMessage::toJson() const {
	nlohmann::json j;

	// Header
	j["header"]["protocolVersion"] = denm->header.protocolVersion;
	j["header"]["messageId"]	   = denm->header.messageID;
	j["header"]["stationId"]	   = denm->header.stationID;

	// Management Container
	auto& mgmt									  = denm->denm.management;
	j["management"]["actionId"]					  = mgmt.actionID.originatingStationID;
	j["management"]["detectionTime"]			  = formatItsTimestamp(mgmt.detectionTime);
	j["management"]["referenceTime"]			  = formatItsTimestamp(mgmt.referenceTime);
	j["management"]["stationType"]				  = mgmt.stationType;
	j["management"]["eventPosition"]["latitude"]  = mgmt.eventPosition.latitude / 10000000.0;
	j["management"]["eventPosition"]["longitude"] = mgmt.eventPosition.longitude / 10000000.0;
	j["management"]["eventPosition"]["altitude"]  = mgmt.eventPosition.altitude.altitudeValue / 100.0;

	// Situation Container (if present)
	if (denm->denm.situation) {
		auto& sit							 = *denm->denm.situation;
		j["situation"]["informationQuality"] = sit.informationQuality;
		j["situation"]["causeCode"]			 = sit.eventType.causeCode;
		j["situation"]["subCauseCode"]		 = sit.eventType.subCauseCode;
	}

	// Location Container (if present)
	if (denm->denm.location) {
		auto& loc = *denm->denm.location;
		if (loc.eventSpeed) {
			j["location"]["eventSpeed"]		 = loc.eventSpeed->speedValue / 100.0;
			j["location"]["speedConfidence"] = loc.eventSpeed->speedConfidence;
		}
		if (loc.eventPositionHeading) {
			j["location"]["eventHeading"]	   = loc.eventPositionHeading->headingValue / 10.0;
			j["location"]["headingConfidence"] = loc.eventPositionHeading->headingConfidence;
		}
	}

	return j;
}

DenmMessage DenmMessage::fromJson(const nlohmann::json& j) {
    DenmMessage msg;
    
    // Header
    msg.denm->header.protocolVersion = j["header"]["protocolVersion"];
    msg.denm->header.messageID = j["header"]["messageId"];
    msg.denm->header.stationID = j["header"]["stationId"];
    
    // Management Container
    auto& mgmt = msg.denm->denm.management;
    mgmt.actionID.originatingStationID = j["management"]["actionId"];
    mgmt.detectionTime = createItsTimestamp(std::time(nullptr));  // Current time
    mgmt.referenceTime = createItsTimestamp(std::time(nullptr));  // Current time
    mgmt.stationType = j["management"]["stationType"];
    
    // Event Position
    mgmt.eventPosition.latitude = static_cast<int32_t>(j["management"]["eventPosition"]["latitude"].get<double>() * 10000000.0);
    mgmt.eventPosition.longitude = static_cast<int32_t>(j["management"]["eventPosition"]["longitude"].get<double>() * 10000000.0);
    mgmt.eventPosition.altitude.altitudeValue = static_cast<int32_t>(j["management"]["eventPosition"]["altitude"].get<double>() * 100.0);
    
    // Optional Situation Container
    if (j.contains("situation")) {
        msg.denm->denm.situation = vanetza::asn1::allocate<SituationContainer_t>();
        auto& sit = *msg.denm->denm.situation;
        sit.informationQuality = j["situation"]["informationQuality"];
        sit.eventType.causeCode = j["situation"]["causeCode"];
        sit.eventType.subCauseCode = j["situation"]["subCauseCode"];
    }
    
    // Optional Location Container
    if (j.contains("location")) {
        msg.denm->denm.location = vanetza::asn1::allocate<LocationContainer_t>();
        auto& loc = *msg.denm->denm.location;
        
        if (j["location"].contains("eventSpeed")) {
            loc.eventSpeed = vanetza::asn1::allocate<Speed_t>();
            loc.eventSpeed->speedValue = static_cast<int16_t>(j["location"]["eventSpeed"].get<double>() * 100.0);
            loc.eventSpeed->speedConfidence = j["location"]["speedConfidence"];
        }
        
        if (j["location"].contains("eventHeading")) {
            loc.eventPositionHeading = vanetza::asn1::allocate<Heading_t>();
            loc.eventPositionHeading->headingValue = static_cast<int16_t>(j["location"]["eventHeading"].get<double>() * 10.0);
            loc.eventPositionHeading->headingConfidence = j["location"]["headingConfidence"];
        }
    }
    
    return msg;
}

std::vector<unsigned char> DenmMessage::getUperEncoded() const {
	std::vector<unsigned char> buffer;
	asn_enc_rval_t ec; // Encoding result

	// Initial buffer size - start with a reasonable size
	size_t buffer_size = 1024; // 1KB should be enough for most DENM messages
	buffer.resize(buffer_size);

	spdlog::debug("Attempting UPER encoding of DENM message");

	// Debug print the key fields before encoding
	spdlog::debug("DENM Header - Protocol Version: {}, Message ID: {}, Station ID: {}",
				  denm->header.protocolVersion,
				  denm->header.messageID,
				  denm->header.stationID);

	spdlog::debug("Management Container - Station Type: {}, Action ID: {}, Traces: {}",
				  denm->denm.management.stationType,
				  denm->denm.management.actionID.sequenceNumber,
				  denm->denm.location ? "present" : "nullptr");

	// Additional debug for mandatory fields
	spdlog::debug("Event Position - Lat: {}, Lon: {}, Alt: {}",
				  denm->denm.management.eventPosition.latitude,
				  denm->denm.management.eventPosition.longitude,
				  denm->denm.management.eventPosition.altitude.altitudeValue);

	// Attempt to encode directly to the buffer
	ec = uper_encode_to_buffer(&asn_DEF_DENM, nullptr, denm.get(), buffer.data(), buffer_size);

	if (ec.encoded == -1) {
		spdlog::error("UPER encoding failed");
		if (ec.failed_type) {
			spdlog::error("Failed type name: {}", ec.failed_type->name);
			spdlog::error("Failed type details: {}", ec.failed_type->xml_tag ?: "unknown");
		}
		throw std::runtime_error("Failed to encode DENM message");
	}

	// Calculate the actual size in bytes (rounding up to nearest byte)
	size_t actual_size = (ec.encoded + 7) / 8;

	spdlog::debug("Successfully encoded DENM message - {} bits ({} bytes)", ec.encoded, actual_size);

	// Resize the buffer to the actual encoded size
	buffer.resize(actual_size);

	// Debug print the first few bytes of encoded data
	std::stringstream hex_output;
	for (size_t i = 0; i < std::min(actual_size, size_t(16)); i++) {
		hex_output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]) << " ";
	}
	spdlog::debug("First 16 bytes of encoded data: {}", hex_output.str());

	return buffer;
}
