#ifndef DENM_MESSAGE_HPP
#define DENM_MESSAGE_HPP

#include <vanetza/asn1/denm.hpp>
#include <vanetza/btp/data_request.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

class DenmMessage {
public:
    DenmMessage();


    // Header setters
    void setProtocolVersion(int version);
    void setMessageId(long messageId);
    void setStationId(long stationId);

    // Management Container setters
    void setActionId(long actionId);
    void setStationType(int type);
    void setDetectionTime(time_t unix_timestamp);
    void setDetectionTime();
    void setReferenceTime(time_t unix_timestamp);
    void setReferenceTime();
    void setTermination(bool isTerminating);
    void setEventPosition(double latitude, double longitude, double altitude);
    void setRelevanceDistance(RelevanceDistance_t distance);
    void setRelevanceTrafficDirection(RelevanceTrafficDirection_t direction);
    void setValidityDuration(std::chrono::seconds duration);
    void setTransmissionInterval(std::chrono::milliseconds interval);

    // Situation Container setters
    void setInformationQuality(int quality);
    void setCauseCode(CauseCodeType_t cause);
    void setSubCauseCode(SubCauseCodeType_t subCause);
    void setEventSpeed(double speedMs);
    void setEventHeading(double headingDegrees);

    // Location Container setters
    void setEventPositionConfidence(double positionConfidence);
    void setEventHeadingConfidence(double headingConfidence);
    void setEventSpeedConfidence(double speedConfidence);

    // Header getters
    int getProtocolVersion() const;
    long getMessageId() const;
    long getStationId() const;

    // Management Container getters
    long getActionId() const;
    TimestampIts_t getDetectionTime() const;
    std::string getDetectionTimeFormatted() const;
    TimestampIts_t getReferenceTime() const;
    std::string getReferenceTimeFormatted() const;
    bool getTermination() const;
    const ReferencePosition_t& getEventPosition() const;
    RelevanceDistance_t getRelevanceDistance() const;
    RelevanceTrafficDirection_t getRelevanceTrafficDirection() const;
    std::chrono::seconds getValidityDuration() const;
    std::chrono::milliseconds getTransmissionInterval() const;
    int getStationType() const;

    // Situation Container getters
    int getInformationQuality() const;
    CauseCodeType_t getCauseCode() const;
    SubCauseCodeType_t getSubCauseCode() const;
    double getEventSpeed() const;
    double getEventHeading() const;

    // Location Container getters
    double getEventPositionConfidence() const;
    double getEventHeadingConfidence() const;
    double getEventSpeedConfidence() const;

    // Build and get the complete DENM message
    vanetza::asn1::Denm buildDenm() const;

    // Deserialize from UPER encoded data
    void fromUper(const std::vector<unsigned char>& data);

private:
    // Add private helper methods for timestamp handling
    static std::string formatItsTimestamp(const TimestampIts_t& timestamp);
    static TimestampIts_t createItsTimestamp(time_t unix_timestamp);
    static const time_t UTC_2004 = 1072915200; // Jan 1, 2004 00:00:00 UTC

    struct Header {
        long protocolVersion;
        long messageId;
        long stationId;
    } header;

    struct ManagementContainer {
        int protocolVersion;
        long actionId;
        TimestampIts_t detectionTime;
        TimestampIts_t referenceTime;
        bool termination;
        int stationType;
        ReferencePosition_t eventPosition;
        RelevanceDistance_t relevanceDistance;
        RelevanceTrafficDirection_t relevanceTrafficDirection;
        std::chrono::seconds validityDuration;
        std::chrono::milliseconds transmissionInterval;
    } management;

    struct SituationContainer {
        int informationQuality;
        CauseCodeType_t causeCode;
        SubCauseCodeType_t subCauseCode;
        double eventSpeed;
        double eventHeading;
    } situation;

    struct LocationContainer {
        double positionConfidence;
        double headingConfidence;
        double speedConfidence;
    } location;
    
};

#endif // DENM_MESSAGE_HPP 