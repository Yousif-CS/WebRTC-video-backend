#include <iostream>
#include <sstream>

#include <WebRTCConnection.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <tutorials/utility_server/step2.cpp>
#include <jwt-cpp/jwt.h>
#include "json/reader.h"

class WsRtcServer: public utility_server {
public:

    //Inherit the utility server constructor
    using utility_server::utility_server;

private:

    //make our own custom events handler 
    //for handling webRTC handshakes
    void event_handler(websocketpp::connection_hdl hdl, server::message_ptr msg) override {

        //getting the data
        std::stringstream ss{msg->get_payload()};
        Json::Value root;
        ss >> root;

        //serving various events
        if (root["message_type"] == "iceCandidate"){
            //root["iceCandidate"]
            set_ice_candidate(root);

        }else if (root["message_type"] == "offer"){
            //set_local_description(root);

        }else if (root["message_type"] == "answer"){
            set_answer(root);
        }
    }

    //override the on connection server
    void on_connection(websocketpp::connection_hdl hd) override {
        
        std::string token = jwt::create()
	        .set_issuer("auth0")
	        .set_type("JWS")
	        .set_payload_claim("sample", jwt::claim(std::string("test")))
	        .sign(jwt::algorithm::hs256{"secret"});
        
        std::vector<std::string> iceservers{"stun:stun.l.google.com:19302"};
        webRtcConnections[token] = new WebRTCConnection(iceservers);
        
        std::cout << "Connection Recieved" << std::endl;

        //establish a base for a connection
        webRtcConnections[token]->establish();

        //Here we set an event handler when we receive an ice candidate
        webRtcConnections[token]->set_on_ice_candidate_handler([this, hd, token](const webrtc::IceCandidateInterface* icecandidate) {
            //we add the ice candidate
            this->webRtcConnections[token]->add_ice_candidate(icecandidate);

            //serialize it and send it over to the client
            Json::Value candidate_payload;
            std::string serialized_ice_candidate;
            icecandidate->ToString(&serialized_ice_candidate);
            candidate_payload["token"] = token;
            candidate_payload["icecandidate"] = serialized_ice_candidate;

            this->send(hd, candidate_payload.asString());

        }); 
        
        //Here we establish a webrtc attempt and start setting up an offer to send
        webRtcConnections[token]->CreateOffer([this, hd, token](webrtc::SessionDescriptionInterface* desc) {
            this->webRtcConnections[token]->setLocalDescription(desc, [this, hd, token, desc](){
                
                std::string serialized_offer;
                desc->ToString(&serialized_offer);
                Json::Value payload;
                payload["message_type"] = "offer";
                payload["token"] = token;
                payload["offer"] = serialized_offer;
                this->send(hd, payload.asString());

            }, [this, hd, token](const std::string& error){
                throw std::runtime_error("Error setting local description: " + error);
            });

        },[this, hd, token](const std::string& error) {
            throw std::runtime_error("Error creating offer: " + error);
        });
        //webRtcConnections[token]->createOffer();
    }

    void set_ice_candidate(Json::Value root){

        webrtc::SdpParseError sdpError;
        webrtc::IceCandidateInterface * icecandidate = webrtc::CreateIceCandidate(root["offer"]["sdp_mid"].asString(), root["offer"]["sdp_mline_index"].asInt(), root["offer"]["sdp"].asString(), &sdpError);
        
        if (icecandidate == nullptr){
            throw std::runtime_error(sdpError.line + ": " + sdpError.description);
        }
        this->webRtcConnections[root["token"].asString()]->add_ice_candidate(icecandidate);
    }

    void set_answer(Json::Value root){
        
        webrtc::SdpParseError sdpError;
        webrtc::SessionDescriptionInterface * sdp = webrtc::CreateSessionDescription(root["answer"]["type"].asString(), root["answer"]["sdp"].asString(), &sdpError);
        
        if (sdp == nullptr){
            throw std::runtime_error(sdpError.line + ": " + sdpError.description);
        }

        this->webRtcConnections[root["token"].asString()]->setRemoteDescription(sdp, [](){
            std::cout << "Setting Remote answer successful!" << std::endl;
        }, [](const std::string& error){
            throw std::runtime_error("Error setting remote answer: " + error);
        });

    }

    std::map<std::string, WebRTCConnection *> webRtcConnections;
};

