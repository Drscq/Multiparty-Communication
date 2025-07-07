#include <zmq.hpp>
#include <thread>
#include <iostream>

int main(int argc, char** argv) {
    zmq::context_t ctx(1);
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <role>" << std::endl;
        std::cerr << "  role: 'router' or 'dealer'" << std::endl;
        return 1;
    }
    std::string role = argv[1];
    if (role == "router") {
        zmq::socket_t router{ctx, zmq::socket_type::router};
        router.bind("tcp://*:5555");
        std::cout << "Router bound to tcp://*:5555" << std::endl;

        while (true) {
            zmq::message_t identity; // identity frame (added by DEALER)
            zmq::message_t payload; // actual data

            auto result1 = router.recv(identity, zmq::recv_flags::none);
            auto result2 = router.recv(payload, zmq::recv_flags::none);
            
            // Suppress unused variable warnings
            (void)result1;
            (void)result2;

            std::cout << "Received message from " << identity.to_string() << ": "
                      << payload.to_string() << std::endl;
            
            // Echo back: ROUTER must re-attach the identify frame
            router.send(identity, zmq::send_flags::sndmore);
            std::string reply = "(" + payload.to_string() + ") ACK";
            router.send(zmq::buffer(reply), zmq::send_flags::none);
        }
    } else if (role == "dealer") {
        zmq::socket_t dealer{ctx, zmq::socket_type::dealer};
        dealer.set(zmq::sockopt::routing_id, "C1");
        dealer.connect("tcp://localhost:5555");

        // Send a single message
        std::string message = "Hello from Dealer C1";
        dealer.send(zmq::buffer(message), zmq::send_flags::none);
        std::cout << "Dealer C1 sent message: " << message << std::endl;

        // Receive reply
        zmq::message_t reply;
        auto result = dealer.recv(reply, zmq::recv_flags::none);
        (void)result; // Suppress unused variable warning
        std::cout << "Dealer C1 received reply: " << reply.to_string() << std::endl;
    }
}