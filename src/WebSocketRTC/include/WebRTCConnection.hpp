#include <iostream>
#include <functional>

#include "peerconnectioninterface.h"
#include "peerconnectionfactoryproxy.h"
#include "peerconnectionproxy.h"
#include "webrtc/base/atomicops.h"

class CreateSessionDescriptionHandlers: public webrtc::CreateSessionDescriptionObserver{
    
public:
    
    CreateSessionDescriptionHandlers(std::function<void(webrtc::SessionDescriptionInterface* desc)> On_success,
    std::function<void(const std::string& error)> On_failure):

    on_failure{On_failure}, on_success{On_success}
    {
        if (! On_success || ! On_failure){
            throw std::runtime_error("Both callback functions must be specified!");
        }
    }

    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        on_success(desc);
    };
    virtual void OnFailure(const std::string& error) override {
        on_failure(error);
    };

    //I have no idea why we need to keep reference count
    virtual int AddRef() const override{
        return rtc::AtomicOps::Increment(&_ref_count);
    }
    virtual int Release() const override {
        int count = rtc::AtomicOps::Decrement(&_ref_count);
        if (!count){
            delete this;
        }
        return count;
    }

private:
    std::function<void(webrtc::SessionDescriptionInterface* desc)> on_success{nullptr};
    std::function<void(const std::string& error)> on_failure{nullptr};
    
    volatile mutable int _ref_count = 0;
};

class SetSessionDescriptionHandlers: public webrtc::SetSessionDescriptionObserver {

public:
    SetSessionDescriptionHandlers(std::function<void()> On_success, std::function<void(const std::string& error)> On_failure)
    : on_success{On_success}, on_failure{On_failure}
    {
        if (!On_success || !On_failure){
            throw std::runtime_error("SetSessionDescriptionHandlers must be specified!");
        }
    }

    void OnSuccess() override {
        on_success();
    }

    void OnFailure(const std::string& error) override{
        on_failure(error);
    }

    //I have no idea why we need to keep reference count
    virtual int AddRef() const override{
        return rtc::AtomicOps::Increment(&_ref_count);
    }
    virtual int Release() const override {
        int count = rtc::AtomicOps::Decrement(&_ref_count);
        if (!count){
            delete this;
        }
        return count;
    }


private:
    std::function<void()> on_success{nullptr};
    std::function<void(const std::string& error)> on_failure{nullptr};
    volatile mutable int _ref_count = 0;
};

//this class here will handle webrtc connection
class PeerConnectionEventHandlers: public webrtc::PeerConnectionObserver{


public:  
    PeerConnectionEventHandlers(std::function<void(const webrtc::IceCandidateInterface * iceCandidate)> f){
        this->on_ice_candidate = f;
    }

private:
    // Triggered when the SignalingState changed.
    virtual void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state){
            std::cout<< "Signal state changed to: "  << resolve_signal_state(new_state) << std::endl;
        };

    // Triggered when media is received on a new stream from remote peer.
    virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){
            std::cout << "Media stream added from peer" << std::endl;
    };

    // Triggered when a remote peer close a stream.
    virtual void OnRemoveStream(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream){
            std::cout << "Stream closed by peer" << std::endl;
        };

    // Triggered when a remote peer opens a data channel.
    virtual void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel){
            std::cout << "Peer has opened a data channel" << std::endl;
        };

    // Triggered when renegotiation is needed. For example, an ICE restart
    // has begun.
    virtual void OnRenegotiationNeeded(){
        std::cout << "Negotiation needed" << std::endl;
    };

    // Called any time the IceConnectionState changes.
    //
    // Note that our ICE states lag behind the standard slightly. The most
    // notable differences include the fact that "failed" occurs after 15
    // seconds, not 30, and this actually represents a combination ICE + DTLS
    // state, so it may be "failed" if DTLS fails while ICE succeeds.
    virtual void OnIceConnectionChange(
        webrtc::PeerConnectionInterface::IceConnectionState new_state){
            if (new_state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted)
                std::cout << "Ice connection state completed" << std::endl; 
        };

    // Called any time the IceGatheringState changes.
    virtual void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state){
            if (new_state == webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete){
                std::cout << "Ice gathering completed" << std::endl;
            }
        };

    // A new ICE candidate has been gathered.
    virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate){
        on_ice_candidate(candidate);
    };

    // Ice candidates have been removed.
    // TODO(honghaiz): Make this a pure virtual method when all its subclasses
    // implement it.
    virtual void OnIceCandidatesRemoved(
        const std::vector<cricket::Candidate>& candidates) {}

    // Called when the ICE connection receiving status changes.
    virtual void OnIceConnectionReceivingChange(bool receiving) {}

private:

    std::function<void(const webrtc::IceCandidateInterface *iceCandidate)> on_ice_candidate;

    std::string resolve_signal_state(webrtc::PeerConnectionInterface::SignalingState state){
        switch(state){
            case webrtc::PeerConnectionInterface::SignalingState::kStable:
                return "Stable";
            case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer:
                return "Have Local Offer";
            case webrtc::PeerConnectionInterface::SignalingState::kHaveLocalPrAnswer:
                return "Have Local Answer";
            case webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer:
                return "Have Remote Offer";
            case webrtc::PeerConnectionInterface::SignalingState::kHaveRemotePrAnswer:
                return "Have Remote Answer";
            case webrtc::PeerConnectionInterface::SignalingState::kClosed:
                return "Closed";
        }
    }
};


class WebRTCConnection{

public:
    //instatiate a connection with a config
    WebRTCConnection(const std::vector<std::string>& Iceservers)
    :iceservers{Iceservers}
    {
        if (iceservers.empty()){
            throw std::runtime_error("List of ice servers is empty");
        }
    }

    void establish(){
        //create a peer connection factory 
        auto pcf = webrtc::CreatePeerConnectionFactory();

        //prepare config for Ice servers
        webrtc::PeerConnectionInterface::RTCConfiguration config;

        //server to hold the urls
        webrtc::PeerConnectionInterface::IceServer server;
        server.urls = iceservers;

        // adding the server to the list of intermediate p2p servers
        config.servers.push_back(server);

        if (!on_ice_candidate){
            throw std::runtime_error("Need to specify callback when ice candidate is received!");
        }

        const PeerConnectionEventHandlers *handlers = new PeerConnectionEventHandlers(on_ice_candidate);
        
        this->peerConnection = pcf->CreatePeerConnection(config, nullptr, nullptr, (webrtc::PeerConnectionObserver *) handlers);


    }

    void CreateOffer(std::function<void(webrtc::SessionDescriptionInterface* desc)> On_success, std::function<void(const std::string &error)> On_failure){

        if (! On_failure || ! On_success){
            throw std::runtime_error("Callbacks must be specified in CreateOffer");
        }

        CreateSessionDescriptionHandlers * sdph = new CreateSessionDescriptionHandlers(On_success, On_failure);
        this->peerConnection->CreateOffer(sdph, nullptr);
    }

    void CreateAnswer(std::function<void(webrtc::SessionDescriptionInterface* desc)> On_success, std::function<void(const std::string &error)> On_failure){
        if (! On_failure || ! On_success){
            throw std::runtime_error("Callbacks must be specified in CreateOffer");
        }

        CreateSessionDescriptionHandlers * sdph = new CreateSessionDescriptionHandlers(On_success, On_failure);
        this->peerConnection->CreateAnswer(sdph, nullptr);

    }

    void setLocalDescription(webrtc::SessionDescriptionInterface* desc, std::function<void()> On_success, std::function<void(const std::string& error)> On_failure){
        SetSessionDescriptionHandlers * sdph = new SetSessionDescriptionHandlers(On_success, On_failure);
        this->peerConnection->SetLocalDescription(sdph, desc);
    }

    void setRemoteDescription(webrtc::SessionDescriptionInterface* desc, std::function<void()> On_success, std::function<void(const std::string& error)> On_failure){
        SetSessionDescriptionHandlers * sdph = new SetSessionDescriptionHandlers(On_success, On_failure);
        this->peerConnection->SetRemoteDescription(sdph, desc);
    }


    //A handler setter for when a new ice candidate is available
    void set_on_ice_candidate_handler(std::function<void(const webrtc::IceCandidateInterface *)> On_ice_candidate){
        if (on_ice_candidate){
            throw std::runtime_error("on_ice_candidate handler already set!");
        }
        on_ice_candidate = On_ice_candidate;
    }

    void add_ice_candidate(const webrtc::IceCandidateInterface * icecandidate){
        this->peerConnection->AddIceCandidate(icecandidate);
    }

private:

    // //A handler setter for when offer creation succeeds
    // void set_on_create_offer_success_handler(void (* On_offer)(webrtc::SessionDescriptionInterface* desc)){
    //     if (on_create_offer_success){
    //         throw std::runtime_error("on_create_offer_success handler already set!");
    //     }
    //     on_create_offer_success = On_offer;
    // }

    // //A handler setter for when offer creation fails
    // void set_on_create_offer_failure_handler(void (* On_failure) (const std::string& error)){
    //     if (On_failure){
    //         throw std::runtime_error("on_create_offer_failure handler already set!");
    //     }

    //     on_create_offer_failure = On_failure;
    // }


    std::function<void (const webrtc::IceCandidateInterface * iceCandidate)> on_ice_candidate{nullptr};

    std::vector<std::string> iceservers;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
};