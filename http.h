#ifndef HTTP_H
#define HTTP_H

#include <boost/asio.hpp>
#include <boost/config.hpp>

#include <exception>
#include <sstream>
#include <string>

namespace http
{
    class exception : public std::exception
    {
    public:
        exception(const std::string& msg) : _msg(msg) {}

        /* virtual */ const char* what() const override BOOST_NOEXCEPT
        {
            return _msg.c_str();
        }

    private:
        const std::string _msg;
    };

    const unsigned int HTTP_OK = 200;

    struct response
    {
        std::string headers;
        std::string message_body;
    };

    enum class REQUEST_TYPES { GET, POST };

    namespace detail
    {
        inline
        response get_response(boost::asio::ip::tcp::socket& socket)
        {
            response server_response;

            // Read the response status line. The response streambuf will automatically
            // grow to accommodate the entire line. The growth may be limited by passing
            // a maximum size to the streambuf constructor.
            boost::asio::streambuf response;
            boost::asio::read_until(socket, response, "\r\n");

            // Check that response is OK.
            std::istream response_stream(&response);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);
            if (!response_stream || http_version.substr(0, 5) != "HTTP/")
            {
                throw exception("An error occurred while doing request - invalid response");
            }
            if (status_code != HTTP_OK)
            {
                std::ostringstream error_desc;
                error_desc << "An error occurred while doing request - response returned with status code " << status_code;
                throw exception(error_desc.str());
            }

            // Read the response headers, which are terminated by a blank line.
            boost::asio::read_until(socket, response, "\r\n\r\n");

            // Process the response headers.
            std::string header;
            while (std::getline(response_stream, header) && header != "\r")
            {
                server_response.headers += header + '\n';
            }

            std::ostringstream message_body;

            // Write whatever content we already have to output.
            if (response.size() > 0)
            {
                message_body << &response;
            }

            // Read until EOF, writing data to output as we go.
            boost::system::error_code error;
            while (boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error))
            {
                message_body << &response;
            }
            if (error != boost::asio::error::eof)
            {
                std::ostringstream error_desc;
                error_desc << "An error occurred while doing request. Value: " << error.value() << " Message: " << error.message();
                throw exception(error_desc.str());
            }

            server_response.message_body = message_body.str();

            return server_response;
        }

        inline
        response request(
            REQUEST_TYPES request_type
            , const std::string& host
            , const std::string& request_uri
            , const std::string& content
            , const std::string& protocol_version
        )
        {
            try
            {
                boost::asio::io_service io_service;

                // Get a list of endpoints corresponding to the server name.
                boost::asio::ip::tcp::resolver resolver(io_service);
                boost::asio::ip::tcp::resolver::query query(host, "http");
                boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

                // Try each endpoint until we successfully establish a connection.
                boost::asio::ip::tcp::socket socket(io_service);
                boost::asio::connect(socket, endpoint_iterator);

                // Form the request. We specify the "Connection: close" header so that the
                // server will close the socket after transmitting the response. This will
                // allow us to treat all data up until the EOF as the content.
                // Also some buggy servers fail in absence of "Accept"
                boost::asio::streambuf request;
                std::ostream request_stream(&request);

                if (request_type == REQUEST_TYPES::GET)
                {
                    request_stream << "GET " << request_uri << ' ' << protocol_version << "\r\n";
                    request_stream << "Host: " << host << "\r\n";
                    request_stream << "Accept: */*\r\n";
                    request_stream << "Connection: close\r\n\r\n";
                }
                else if (request_type == REQUEST_TYPES::POST)
                {
                    request_stream << "POST " << request_uri << ' ' << protocol_version << "\r\n";
                    request_stream << "Host: " << host << "\r\n";
                    request_stream << "Accept: */*\r\n";
                    request_stream << "Connection: close\r\n";
                    request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
                    request_stream << "Content-Length: " << content.size() << "\r\n\r\n";
                    request_stream << content;
                }
                else
                {
                    throw exception("Unsupported HTTP request type");
                }

                // Send the request.
                boost::asio::write(socket, request);

                return get_response(socket);
            }
            catch (const std::exception& ex)
            {
                throw exception(ex.what());
            }
        }
    }

    inline
    response get_request(
        const std::string& host
        , const std::string& request_uri
        , const std::string& protocol_version = "HTTP/1.0"
    )
    {
        return detail::request(REQUEST_TYPES::GET, host, request_uri, "", protocol_version);
    }

    inline
    response post_request(
        const std::string& host
        , const std::string& request_uri
        , const std::string& content
        , const std::string& protocol_version = "HTTP/1.0"
    )
    {
        return detail::request(REQUEST_TYPES::POST, host, request_uri, content, protocol_version);
    }
}

#endif // HTTP_H
