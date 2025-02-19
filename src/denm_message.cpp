#include "denm_message.hpp"
#include <vanetza/units/angle.hpp>
#include <vanetza/units/velocity.hpp>
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>

// Add private helper function declarations to the header first

// Add at the top with other constants
static const time_t UTC_2004 = 1072915200; // Jan 1, 2004 00:00:00 UTC

DenmMessage::DenmMessage() {
    // Initialize Management Container with default values
    management.actionId = 0;
    management.detectionTime = TimestampIts_t();
    management.referenceTime = TimestampIts_t();
    management.termination = false;
    management.eventPosition = ReferencePosition_t{};
    management.relevanceDistance = RelevanceDistance_lessThan50m;
    management.relevanceTrafficDirection = RelevanceTrafficDirection_allTrafficDirections;
    management.validityDuration = std::chrono::seconds(600); // 10 minutes default
    management.transmissionInterval = std::chrono::milliseconds(1000); // 1 second default

    // Initialize Situation Container with default values
    situation.informationQuality = 1;
    situation.causeCode = CauseCodeType_dangerousSituation;
    situation.subCauseCode = 0;
    situation.eventSpeed = 0.0;
    situation.eventHeading = 0.0;

    // Initialize Location Container with default values
    location.positionConfidence = 0.95;
    location.headingConfidence = 0.95;
    location.speedConfidence = 0.95;
}

// Header setters implementation
void DenmMessage::setProtocolVersion(int version) {
    header.protocolVersion = version;
}

void DenmMessage::setMessageId(long messageId) {
    header.messageId = messageId;
}

void DenmMessage::setStationId(long stationId) {
    header.stationId = stationId;
}

// Management Container setters implementation

void DenmMessage::setStationType(int type) {
    management.stationType = type;
}

void DenmMessage::setActionId(long actionId) {
    management.actionId = actionId;
}

void DenmMessage::setDetectionTime(time_t unix_timestamp) {
    management.detectionTime = createItsTimestamp(unix_timestamp);
}

void DenmMessage::setDetectionTime() {
    setDetectionTime(std::time(nullptr));
}

void DenmMessage::setReferenceTime(time_t unix_timestamp) {
    management.referenceTime = createItsTimestamp(unix_timestamp);
}

void DenmMessage::setReferenceTime() {
    setReferenceTime(std::time(nullptr));
}

void DenmMessage::setTermination(bool isTerminating) {
    management.termination = isTerminating;
}

void DenmMessage::setEventPosition(double latitude, double longitude, double altitude) {
    management.eventPosition.latitude = latitude * 10000000; // Convert to 1/10 micro degree
    management.eventPosition.longitude = longitude * 10000000;
    management.eventPosition.altitude.altitudeValue = altitude * 100; // Convert to centimeters
}

void DenmMessage::setRelevanceDistance(RelevanceDistance_t distance) {
    management.relevanceDistance = distance;
}

void DenmMessage::setRelevanceTrafficDirection(RelevanceTrafficDirection_t direction) {
    management.relevanceTrafficDirection = direction;
}

void DenmMessage::setValidityDuration(std::chrono::seconds duration) {
    management.validityDuration = duration;
}

void DenmMessage::setTransmissionInterval(std::chrono::milliseconds interval) {
    management.transmissionInterval = interval;
}

// Situation Container setters implementation
void DenmMessage::setInformationQuality(int quality) {
    situation.informationQuality = quality;
}

void DenmMessage::setCauseCode(CauseCodeType_t cause) {
    situation.causeCode = cause;
}

void DenmMessage::setSubCauseCode(SubCauseCodeType_t subCause) {
    situation.subCauseCode = subCause;
}

void DenmMessage::setEventSpeed(double speedMs) {
    situation.eventSpeed = speedMs;
}

void DenmMessage::setEventHeading(double headingDegrees) {
    situation.eventHeading = headingDegrees;
}

// Location Container setters implementation
void DenmMessage::setEventPositionConfidence(double positionConfidence) {
    location.positionConfidence = positionConfidence;
}

void DenmMessage::setEventHeadingConfidence(double headingConfidence) {
    location.headingConfidence = headingConfidence;
}

void DenmMessage::setEventSpeedConfidence(double speedConfidence) {
    location.speedConfidence = speedConfidence;
}

// Getters implementation

// Header getters
int DenmMessage::getProtocolVersion() const {
    return header.protocolVersion;
}

long DenmMessage::getMessageId() const {
    return header.messageId;
}

long DenmMessage::getStationId() const {
    return header.stationId;
}

// Management Container getters
long DenmMessage::getActionId() const {
    return management.actionId;
}


int DenmMessage::getStationType() const {
    return management.stationType;
}

TimestampIts_t DenmMessage::getDetectionTime() const {
    return management.detectionTime;
}

TimestampIts_t DenmMessage::getReferenceTime() const {
    return management.referenceTime;
}

bool DenmMessage::getTermination() const {
    return management.termination;
}

const ReferencePosition_t& DenmMessage::getEventPosition() const {
    return management.eventPosition;
}

RelevanceDistance_t DenmMessage::getRelevanceDistance() const {
    return management.relevanceDistance;
}

RelevanceTrafficDirection_t DenmMessage::getRelevanceTrafficDirection() const {
    return management.relevanceTrafficDirection;
}

std::chrono::seconds DenmMessage::getValidityDuration() const {
    return management.validityDuration;
}

std::chrono::milliseconds DenmMessage::getTransmissionInterval() const {
    return management.transmissionInterval;
}

// Situation Container getters
int DenmMessage::getInformationQuality() const {
    return situation.informationQuality;
}

CauseCodeType_t DenmMessage::getCauseCode() const {
    return situation.causeCode;
}

SubCauseCodeType_t DenmMessage::getSubCauseCode() const {
    return situation.subCauseCode;
}

double DenmMessage::getEventSpeed() const {
    return situation.eventSpeed;
}

double DenmMessage::getEventHeading() const {
    return situation.eventHeading;
}

// Location Container getters
double DenmMessage::getEventPositionConfidence() const {
    return location.positionConfidence;
}

double DenmMessage::getEventHeadingConfidence() const {
    return location.headingConfidence;
}

double DenmMessage::getEventSpeedConfidence() const {
    return location.speedConfidence;
}

std::string DenmMessage::getDetectionTimeFormatted() const {
    return formatItsTimestamp(management.detectionTime);
}

std::string DenmMessage::getReferenceTimeFormatted() const {
    return formatItsTimestamp(management.referenceTime);
}

vanetza::asn1::Denm DenmMessage::buildDenm() const {
    vanetza::asn1::Denm denm;
    
    // Set Header
    denm->header.protocolVersion = 2;  // Current protocol version
    denm->header.messageID = ItsPduHeader__messageID_denm;  // DENM message ID
    denm->header.stationID = header.stationId;  // set station ID to 1
    
    // Set Management Container
    auto& mgmt = denm->denm.management;  // Direct access to management container
    
    // ActionID
    mgmt.actionID.originatingStationID = management.actionId;
    mgmt.actionID.sequenceNumber = 0; // Default sequence number

    // Station Type
    mgmt.stationType = management.stationType;
    
    // Detection and Reference Time
    mgmt.detectionTime = management.detectionTime;
    mgmt.referenceTime = management.referenceTime;
    
    // Termination (optional)
    if (management.termination) {
        mgmt.termination = vanetza::asn1::allocate<Termination_t>();
        *mgmt.termination = Termination_isCancellation;
    }
    
    // Event Position
    mgmt.eventPosition = management.eventPosition;
    
    // Relevance Distance (optional)
    mgmt.relevanceDistance = vanetza::asn1::allocate<RelevanceDistance_t>();
    *mgmt.relevanceDistance = management.relevanceDistance;
    
    // Relevance Traffic Direction (optional)
    mgmt.relevanceTrafficDirection = vanetza::asn1::allocate<RelevanceTrafficDirection_t>();
    *mgmt.relevanceTrafficDirection = management.relevanceTrafficDirection;
    
    // Validity Duration
    mgmt.validityDuration = vanetza::asn1::allocate<ValidityDuration_t>();
    *mgmt.validityDuration = management.validityDuration.count();
    
    // Transmission Interval (optional)
    mgmt.transmissionInterval = vanetza::asn1::allocate<TransmissionInterval_t>();
    *mgmt.transmissionInterval = management.transmissionInterval.count();
    
    // Set Situation Container (optional)
    denm->denm.situation = vanetza::asn1::allocate<SituationContainer_t>();
    auto& sit = *denm->denm.situation;
    
    // Information Quality
    sit.informationQuality = situation.informationQuality;
    
    // Event Type
    sit.eventType.causeCode = situation.causeCode;
    sit.eventType.subCauseCode = situation.subCauseCode;
    
    // Set Location Container (optional)
    denm->denm.location = vanetza::asn1::allocate<LocationContainer_t>();
    auto& loc = *denm->denm.location;
    
    // Event Speed
    loc.eventSpeed = vanetza::asn1::allocate<Speed>();
    // Speed value should be in 0.01 m/s units (0..16383)
    long speedValue = static_cast<long>(situation.eventSpeed * 100);
    speedValue = std::min(std::max(speedValue, 0L), 16383L);
    loc.eventSpeed->speedValue = speedValue;
    
    long speedConfidence = static_cast<long>(location.speedConfidence);
    speedConfidence = std::min(std::max(speedConfidence, 1L), 100L);
    loc.eventSpeed->speedConfidence = speedConfidence;
    
    // Event Position Heading
    loc.eventPositionHeading = vanetza::asn1::allocate<Heading>();
    // Heading value should be in 0.1 degree units (0..3601)
    long headingValue = static_cast<long>(situation.eventHeading * 10);
    headingValue = std::min(std::max(headingValue, 0L), 3601L);
    loc.eventPositionHeading->headingValue = headingValue;
    
    long headingConfidence = static_cast<long>(location.headingConfidence);
    headingConfidence = std::min(std::max(headingConfidence, 1L), 100L);
    loc.eventPositionHeading->headingConfidence = headingConfidence;

    // Initialize Traces with one empty PathHistory
    loc.traces.list.size = 1;
    loc.traces.list.count = 1;
    loc.traces.list.array = static_cast<struct PathHistory**>(
        vanetza::asn1::allocate(sizeof(struct PathHistory*))
    );
    
    // Allocate and initialize one PathHistory entry
    loc.traces.list.array[0] = static_cast<struct PathHistory*>(
        vanetza::asn1::allocate(sizeof(struct PathHistory))
    );
    
    // Initialize the PathHistory
    PathHistory_t* path = loc.traces.list.array[0];
    path->list.count = 0;
    path->list.size = 0;
    path->list.array = static_cast<struct PathPoint**>(
        vanetza::asn1::allocate(0)
    );

    return denm;
}

// Helper function implementation
std::string DenmMessage::formatItsTimestamp(const TimestampIts_t& timestamp) {
    int64_t msec_since_2004;
    if (asn_INTEGER2long(&timestamp, &msec_since_2004) != 0) {
        throw std::runtime_error("Failed to decode ITS timestamp");
    }
    
    time_t unix_timestamp = (msec_since_2004 / 1000) + UTC_2004;
    
    // Validate the resulting timestamp
    if (unix_timestamp < UTC_2004) {
        throw std::runtime_error("Invalid ITS timestamp (before 2004)");
    }
    
    std::stringstream ss;
    auto tm = std::gmtime(&unix_timestamp);
    if (!tm) {
        throw std::runtime_error("Failed to convert timestamp to UTC");
    }
    ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

TimestampIts_t DenmMessage::createItsTimestamp(time_t unix_timestamp) {
    // Ensure the timestamp is within valid range
    if (unix_timestamp < UTC_2004) {
        throw std::runtime_error("Timestamp before ITS epoch (2004-01-01)");
    }
    
    int64_t msec_since_2004 = static_cast<int64_t>(unix_timestamp - UTC_2004) * 1000;
    TimestampIts_t timestamp = {};
    if (asn_long2INTEGER(&timestamp, msec_since_2004) != 0) {
        throw std::runtime_error("Failed to convert timestamp");
    }
    return timestamp;
}

void DenmMessage::fromUper(const std::vector<unsigned char>& data) {
    DENM_t* denm = nullptr;
    asn_dec_rval_t rval;
    
    spdlog::debug("Starting UPER decoding of {} bytes", data.size());
    
    // Decode UPER data into DENM structure
    rval = uper_decode_complete(
        nullptr,
        &asn_DEF_DENM,
        (void**)&denm,
        data.data(),
        data.size()
    );

    if (rval.code != RC_OK) {
        spdlog::error("UPER decoding failed with code: {}", rval.code);
        ASN_STRUCT_FREE(asn_DEF_DENM, denm);
        throw std::runtime_error("Failed to decode UPER data");
    }

    spdlog::debug("UPER decoding successful");

    // Check message ID
    if (denm->header.messageID != ItsPduHeader__messageID_denm) {
        spdlog::error("Invalid message ID: {} (expected {})", 
                     denm->header.messageID, ItsPduHeader__messageID_denm);
        ASN_STRUCT_FREE(asn_DEF_DENM, denm);
        throw std::runtime_error("Invalid message ID in decoded DENM");
    }

    try {

        // Header
        header.protocolVersion = denm->header.protocolVersion;
        header.messageId = denm->header.messageID;
        header.stationId = denm->header.stationID;

        // Management Container
        auto& mgmt = denm->denm.management;
        management.actionId = mgmt.actionID.originatingStationID;
        management.detectionTime = mgmt.detectionTime;
        management.referenceTime = mgmt.referenceTime;
        management.eventPosition = mgmt.eventPosition;
        management.stationType = mgmt.stationType;
        spdlog::debug("Decoded Management Container successfully");
        
        if (mgmt.relevanceDistance) {
            management.relevanceDistance = *mgmt.relevanceDistance;
        }
        if (mgmt.validityDuration) {
            management.validityDuration = std::chrono::seconds(*mgmt.validityDuration);
        }

        // Situation Container
        if (denm->denm.situation) {
            spdlog::debug("Decoding Situation Container");
            auto& sit = *denm->denm.situation;
            situation.informationQuality = sit.informationQuality;
            situation.causeCode = sit.eventType.causeCode;
            situation.subCauseCode = sit.eventType.subCauseCode;
            spdlog::debug("Decoded Situation Container successfully");
        } else {
            spdlog::debug("No Situation Container present");
        }

        // Location Container
        if (denm->denm.location) {
            spdlog::debug("Decoding Location Container");
            auto& loc = *denm->denm.location;
            if (loc.eventSpeed) {
                // Speed value is in 0.01 m/s units
                situation.eventSpeed = static_cast<double>(loc.eventSpeed->speedValue) / 100.0;
                // Speed confidence is already in percentage (1-100)
                location.speedConfidence = static_cast<double>(loc.eventSpeed->speedConfidence);
                spdlog::debug("Decoded speed: {} m/s, confidence: {}", 
                            situation.eventSpeed, location.speedConfidence);
            }
            if (loc.eventPositionHeading) {
                // Heading value is in 0.1 degree units
                situation.eventHeading = static_cast<double>(loc.eventPositionHeading->headingValue) / 10.0;
                // Heading confidence is already in percentage (1-100)
                location.headingConfidence = static_cast<double>(loc.eventPositionHeading->headingConfidence);
                spdlog::debug("Decoded heading: {} degrees, confidence: {}", 
                            situation.eventHeading, location.headingConfidence);
            }
            // Position confidence is already in percentage (1-100)
            location.positionConfidence = location.positionConfidence;
            
            spdlog::debug("Decoded Location Container successfully");
        } else {
            spdlog::debug("No Location Container present");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error during DENM decoding: {}", e.what());
        ASN_STRUCT_FREE(asn_DEF_DENM, denm);
        throw;
    }

    // Create a single log message with all the details
    std::stringstream logMessage;
    logMessage << "Received and decoded DENM message:\n";

    // Header
    logMessage << "Header:\n"
              << "  Protocol Version: " << header.protocolVersion << "\n"
              << "  Message ID: " << header.messageId << "\n"
              << "  Station ID: " << header.stationId << "\n";
    
    // Management Container
    logMessage << "Management Container:\n"
              << "  Action ID: " << management.actionId << "\n"
              << "  Detection Time: " << getDetectionTimeFormatted() << "\n"
              << "  Reference Time: " << getReferenceTimeFormatted() << "\n"
              << "  Protocol Version: " << denm->header.protocolVersion << "\n"
              << "  Event Position: " << management.eventPosition.latitude << ", " << management.eventPosition.longitude << ", " << management.eventPosition.altitude.altitudeValue << "\n"
              << "  Station Type: " << management.stationType << "\n"
              << "  Termination: " << (management.termination ? "true" : "false") << "\n"
              << "  Validity Duration: " << management.validityDuration.count() << "s\n";

    // Situation Container
    logMessage << "Situation Container:\n"
              << "  Information Quality: " << situation.informationQuality << "\n"
              << "  Cause Code: " << situation.causeCode << "\n"
              << "  Sub Cause Code: " << situation.subCauseCode << "\n"
              << "  Event Speed: " << situation.eventSpeed << " m/s\n"
              << "  Event Heading: " << situation.eventHeading << " degrees\n";

    // Location Container
    logMessage << "Location Container:\n"
              << "  Position Confidence: " << location.positionConfidence << "\n"
              << "  Heading Confidence: " << location.headingConfidence << "\n"
              << "  Speed Confidence: " << location.speedConfidence;

    // Log everything in a single call
    spdlog::debug(logMessage.str());

    // Free the decoded structure
    ASN_STRUCT_FREE(asn_DEF_DENM, denm);
    spdlog::debug("DENM decoding completed successfully");
}
