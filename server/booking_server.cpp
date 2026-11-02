#include "AdminModule.h"
#include "BookingModule.h"
#include "FareModule.h"
#include "FileModule.h"
#include "TicketModule.h"
#include "TrainModule.h"
#include "ValidationModule.h"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr int kDefaultPort = 8080;
constexpr std::size_t kMaxHeaderBytes = 32 * 1024;
constexpr std::size_t kMaxBodyBytes = 64 * 1024;

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

void skipWhitespace(const std::string& input, std::size_t& position) {
    while (position < input.size() &&
           std::isspace(static_cast<unsigned char>(input[position]))) {
        ++position;
    }
}

bool appendUtf8(unsigned int codePoint, std::string& output) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0x10FFFF) {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        return false;
    }
    return true;
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

bool parseJsonString(const std::string& input, std::size_t& position,
                     std::string& output) {
    if (position >= input.size() || input[position] != '"') return false;
    ++position;
    output.clear();

    while (position < input.size()) {
        const unsigned char ch = static_cast<unsigned char>(input[position++]);
        if (ch == '"') return true;
        if (ch < 0x20) return false;
        if (ch != '\\') {
            output.push_back(static_cast<char>(ch));
            continue;
        }
        if (position >= input.size()) return false;

        const char escaped = input[position++];
        switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                if (position + 4 > input.size()) return false;
                unsigned int codePoint = 0;
                for (int i = 0; i < 4; ++i) {
                    const int digit = hexValue(input[position++]);
                    if (digit < 0) return false;
                    codePoint = (codePoint << 4) | static_cast<unsigned int>(digit);
                }
                // The booking validator accepts ASCII names. Preserve valid
                // BMP escapes as UTF-8; validation will reject unsupported names.
                if (codePoint >= 0xD800 && codePoint <= 0xDFFF) return false;
                if (!appendUtf8(codePoint, output)) return false;
                break;
            }
            default: return false;
        }
    }
    return false;
}

bool parseJsonObject(const std::string& input,
                     std::map<std::string, std::string>& values) {
    std::size_t position = 0;
    skipWhitespace(input, position);
    if (position >= input.size() || input[position++] != '{') return false;

    skipWhitespace(input, position);
    if (position < input.size() && input[position] == '}') {
        ++position;
        skipWhitespace(input, position);
        return position == input.size();
    }

    while (position < input.size()) {
        std::string key;
        if (!parseJsonString(input, position, key)) return false;
        skipWhitespace(input, position);
        if (position >= input.size() || input[position++] != ':') return false;
        skipWhitespace(input, position);

        std::string value;
        if (position < input.size() && input[position] == '"') {
            if (!parseJsonString(input, position, value)) return false;
        } else {
            const std::size_t start = position;
            while (position < input.size() && input[position] != ',' &&
                   input[position] != '}' &&
                   !std::isspace(static_cast<unsigned char>(input[position]))) {
                ++position;
            }
            if (start == position) return false;
            value = input.substr(start, position - start);
        }
        values[key] = value;
        skipWhitespace(input, position);

        if (position < input.size() && input[position] == ',') {
            ++position;
            skipWhitespace(input, position);
            continue;
        }
        if (position < input.size() && input[position] == '}') {
            ++position;
            skipWhitespace(input, position);
            return position == input.size();
        }
        return false;
    }
    return false;
}

bool parsePositiveInt(const std::string& text, int& value) {
    if (text.empty()) return false;
    for (unsigned char ch : text) {
        if (!std::isdigit(ch)) return false;
    }
    try {
        std::size_t used = 0;
        const long parsed = std::stol(text, &used);
        if (used != text.size() || parsed <= 0 || parsed > 2147483647L) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream output;
    for (unsigned char ch : value) {
        switch (ch) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (ch < 0x20) {
                    const char hex[] = "0123456789abcdef";
                    output << "\\u00" << hex[ch >> 4] << hex[ch & 0x0F];
                } else {
                    output << static_cast<char>(ch);
                }
        }
    }
    return output.str();
}

std::string jsonString(const std::string& value) {
    return "\"" + jsonEscape(value) + "\"";
}

bool sendAll(int client, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t count = send(client, data.data() + sent,
                                   data.size() - sent, 0);
        if (count < 0) return false;
        if (count == 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

void sendResponse(int client, int statusCode, const std::string& reason,
                  const std::string& contentType, const std::string& body) {
    std::ostringstream header;
    header << "HTTP/1.1 " << statusCode << ' ' << reason << "\r\n"
           << "Content-Type: " << contentType << "; charset=utf-8\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Cache-Control: no-store\r\n"
           << "X-Content-Type-Options: nosniff\r\n"
           << "Connection: close\r\n\r\n";
    sendAll(client, header.str() + body);
}

bool receiveRequest(int client, HttpRequest& request) {
    std::string raw;
    char buffer[4096];
    std::size_t headerEnd = std::string::npos;
    std::size_t contentLength = 0;

    while (headerEnd == std::string::npos) {
        const ssize_t count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        raw.append(buffer, static_cast<std::size_t>(count));
        if (raw.size() > kMaxHeaderBytes) return false;
        headerEnd = raw.find("\r\n\r\n");
    }

    const std::string headers = raw.substr(0, headerEnd);
    std::istringstream lines(headers);
    std::string requestLine;
    if (!std::getline(lines, requestLine)) return false;
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    std::istringstream requestParts(requestLine);
    std::string version;
    if (!(requestParts >> request.method >> request.path >> version)) return false;

    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = toLower(line.substr(0, colon));
        if (name == "content-length") {
            const std::string lengthText = line.substr(colon + 1);
            try {
                std::size_t used = 0;
                const unsigned long parsed = std::stoul(lengthText, &used);
                if (used != lengthText.size() || parsed > kMaxBodyBytes) return false;
                contentLength = static_cast<std::size_t>(parsed);
            } catch (...) {
                return false;
            }
        }
    }

    const std::size_t bodyStart = headerEnd + 4;
    while (raw.size() < bodyStart + contentLength) {
        const ssize_t count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        raw.append(buffer, static_cast<std::size_t>(count));
        if (raw.size() > bodyStart + kMaxBodyBytes) return false;
    }

    const std::size_t query = request.path.find('?');
    if (query != std::string::npos) request.path.resize(query);
    request.body = raw.substr(bodyStart, contentLength);
    return true;
}

void sendJsonError(int client, int status, const std::string& reason,
                   const std::string& message) {
    sendResponse(client, status, reason, "application/json",
        "{\"success\":false,\"message\":" + jsonString(message) + "}");
}

std::string trainToJson(const Train& train) {
    std::ostringstream output;
    output << "{"
           << "\"trainNo\":" << train.trainNo << ','
           << "\"name\":" << jsonString(train.name) << ','
           << "\"source\":" << jsonString(train.source) << ','
           << "\"destination\":" << jsonString(train.destination) << ','
           << "\"departure\":" << jsonString(train.departure) << ','
           << "\"arrival\":" << jsonString(train.arrival) << ','
           << "\"fare\":" << train.fare << ','
           << "\"totalSeats\":" << train.totalSeats << ','
           << "\"availableSeats\":" << train.availableSeats
           << '}';
    return output.str();
}

void handleTrains(int client, const TrainModule& trains) {
    std::ostringstream body;
    body << "{\"success\":true,\"trains\":[";
    const std::vector<Train>& records = trains.list();
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i != 0) body << ',';
        body << trainToJson(records[i]);
    }
    body << "]}";
    sendResponse(client, 200, "OK", "application/json", body.str());
}

void handleBooking(int client, const std::string& body,
                   BookingModule& bookingModule, TrainModule& trains) {
    std::map<std::string, std::string> values;
    if (!parseJsonObject(body, values)) {
        sendJsonError(client, 400, "Bad Request", "Request body must be a valid JSON object.");
        return;
    }

    const auto trainField = values.find("trainNo");
    const auto nameField = values.find("name");
    const auto ageField = values.find("age");
    const auto genderField = values.find("gender");
    const auto contactField = values.find("contact");

    if (trainField == values.end() || nameField == values.end() ||
        ageField == values.end() || genderField == values.end() ||
        contactField == values.end()) {
        sendJsonError(client, 400, "Bad Request", "Required fields: trainNo, name, age, gender and contact.");
        return;
    }

    int trainNo = 0;
    int age = 0;
    if (!parsePositiveInt(trainField->second, trainNo) ||
        !parsePositiveInt(ageField->second, age) ||
        genderField->second.size() != 1) {
        sendJsonError(client, 400, "Bad Request", "Train number, age or gender is invalid.");
        return;
    }

    Passenger passenger;
    passenger.name = nameField->second;
    passenger.age = age;
    passenger.gender = genderField->second.front();
    passenger.contact = contactField->second;

    Booking created;
    const Status result = bookingModule.createBooking(trainNo, passenger, created);
    if (result != Status::OK) {
        switch (result) {
            case Status::INVALID_INPUT:
                sendJsonError(client, 400, "Bad Request",
                    "Passenger details are invalid. Check the name, age, gender and 10-digit mobile number (starting 6-9).");
                return;
            case Status::TRAIN_NOT_FOUND:
                sendJsonError(client, 404, "Not Found", "Train number was not found.");
                return;
            case Status::NO_SEATS:
                sendJsonError(client, 409, "Conflict", "No seats are available on this train.");
                return;
            case Status::PNR_LIMIT_REACHED:
                sendJsonError(client, 503, "Service Unavailable", "A PNR could not be generated. Please try again later.");
                return;
            case Status::FILE_ERROR:
                sendJsonError(client, 500, "Internal Server Error", "The booking could not be saved. No confirmation was created.");
                return;
            default:
                sendJsonError(client, 400, "Bad Request", "The booking could not be completed.");
                return;
        }
    }

    Train train;
    if (trains.findByNumber(created.trainNo, train) != Status::OK) {
        sendJsonError(client, 500, "Internal Server Error", "Booking was saved, but train details could not be loaded.");
        return;
    }

    std::ostringstream response;
    response << "{\"success\":true,\"message\":\"Booking created successfully.\",\"booking\":{"
             << "\"pnr\":" << jsonString(created.pnr) << ','
             << "\"trainNo\":" << created.trainNo << ','
             << "\"trainName\":" << jsonString(train.name) << ','
             << "\"source\":" << jsonString(train.source) << ','
             << "\"destination\":" << jsonString(train.destination) << ','
             << "\"departure\":" << jsonString(train.departure) << ','
             << "\"arrival\":" << jsonString(train.arrival) << ','
             << "\"passenger\":{"
             << "\"name\":" << jsonString(created.passenger.name) << ','
             << "\"age\":" << created.passenger.age << ','
             << "\"gender\":" << jsonString(std::string(1, created.passenger.gender)) << ','
             << "\"contact\":" << jsonString(created.passenger.contact) << "},"
             << "\"seatNo\":" << created.seatNo << ','
             << "\"fare\":" << created.fare << ','
             << "\"bookingDate\":" << jsonString(created.bookingDate) << ','
             << "\"status\":" << jsonString(created.status)
             << "}}";

    sendResponse(client, 201, "Created", "application/json", response.str());
    std::cout << "Created booking PNR " << created.pnr
              << " for train " << created.trainNo << '\n';
}

// Convert a booking into the JSON structure used by the web interface.
std::string bookingToJson(const Booking& booking,
                          const TrainModule& trains) {
    Train train;
    const bool trainFound =
        trains.findByNumber(booking.trainNo, train) == Status::OK;

    std::ostringstream output;
    output << "{"
           << "\"pnr\":" << jsonString(booking.pnr) << ','
           << "\"trainNo\":" << booking.trainNo << ','
           << "\"trainName\":"
           << jsonString(trainFound ? train.name : "") << ','
           << "\"source\":"
           << jsonString(trainFound ? train.source : "") << ','
           << "\"destination\":"
           << jsonString(trainFound ? train.destination : "") << ','
           << "\"departure\":"
           << jsonString(trainFound ? train.departure : "") << ','
           << "\"arrival\":"
           << jsonString(trainFound ? train.arrival : "") << ','
           << "\"passenger\":{"
           << "\"name\":" << jsonString(booking.passenger.name) << ','
           << "\"age\":" << booking.passenger.age << ','
           << "\"gender\":"
           << jsonString(std::string(1, booking.passenger.gender)) << ','
           << "\"contact\":"
           << jsonString(booking.passenger.contact)
           << "},"
           << "\"seatNo\":" << booking.seatNo << ','
           << "\"fare\":" << booking.fare << ','
           << "\"bookingDate\":"
           << jsonString(booking.bookingDate) << ','
           << "\"status\":" << jsonString(booking.status) << ','
           << "\"refund\":" << booking.refund << ','
           << "\"cancellationCharge\":"
           << (booking.status == "CANCELLED"
                   ? booking.fare - booking.refund : 0.0)
           << '}';

    return output.str();
}

// Retrieve a booking using its PNR.
void handleBookingLookup(int client, const std::string& pnr,
                         const TicketModule& tickets,
                         const TrainModule& trains) {
    Booking booking;
    const Status result = tickets.find(pnr, booking);

    if (result == Status::INVALID_PNR) {
        sendJsonError(client, 400, "Bad Request",
                      "Please enter a valid 10-digit PNR.");
        return;
    }

    if (result == Status::PNR_NOT_FOUND) {
        sendJsonError(client, 404, "Not Found",
                      "No booking was found for this PNR.");
        return;
    }

    if (result != Status::OK) {
        sendJsonError(client, 500, "Internal Server Error",
                      "Could not retrieve booking details.");
        return;
    }

    const std::string response =
        "{\"success\":true,\"booking\":" +
        bookingToJson(booking, trains) + "}";
    sendResponse(client, 200, "OK", "application/json", response);
}

// Cancel a booking and return the updated booking, including its refund.
void handleCancellation(int client, const std::string& body,
                        BookingModule& bookingModule,
                        const TicketModule& tickets,
                        const TrainModule& trains) {
    std::map<std::string, std::string> values;
    if (!parseJsonObject(body, values)) {
        sendJsonError(client, 400, "Bad Request",
                      "Request body must be a valid JSON object.");
        return;
    }

    const auto pnrField = values.find("pnr");
    if (pnrField == values.end()) {
        sendJsonError(client, 400, "Bad Request", "A PNR is required.");
        return;
    }

    const Status result = bookingModule.cancelBooking(pnrField->second);
    if (result != Status::OK) {
        switch (result) {
            case Status::INVALID_PNR:
                sendJsonError(client, 400, "Bad Request",
                              "Please enter a valid 10-digit PNR.");
                return;
            case Status::PNR_NOT_FOUND:
                sendJsonError(client, 404, "Not Found",
                              "No booking was found for this PNR.");
                return;
            case Status::ALREADY_CANCELLED:
                sendJsonError(client, 409, "Conflict",
                              "This booking has already been cancelled.");
                return;
            case Status::TRAIN_NOT_FOUND:
                sendJsonError(client, 404, "Not Found",
                              "The associated train could not be found.");
                return;
            case Status::FILE_ERROR:
                sendJsonError(client, 500, "Internal Server Error",
                              "Cancellation could not be saved.");
                return;
            case Status::INVALID_INPUT:
                sendJsonError(client, 409, "Conflict",
                              "Only confirmed bookings can be cancelled.");
                return;
            default:
                sendJsonError(client, 500, "Internal Server Error",
                              "The booking could not be cancelled.");
                return;
        }
    }

    Booking cancelledBooking;
    const Status lookupStatus =
        tickets.find(pnrField->second, cancelledBooking);
    if (lookupStatus != Status::OK) {
        sendJsonError(client, 500, "Internal Server Error",
                      "Cancellation was saved, but updated booking details could not be loaded.");
        return;
    }

    const std::string response =
        "{\"success\":true,"
        "\"message\":\"Booking cancelled successfully.\","
        "\"booking\":" +
        bookingToJson(cancelledBooking, trains) + "}";
    sendResponse(client, 200, "OK", "application/json", response);

    std::cout << "Cancelled booking PNR "
              << cancelledBooking.pnr << '\n';
}

// Return the current administrator login state to the browser.
void handleAdminSession(int client, const AdminModule& adminModule,
                        int failedLoginAttempts) {
    const int remainingAttempts =
        std::max(0, AdminModule::kMaxLoginAttempts - failedLoginAttempts);
    const std::string body =
        std::string("{\"success\":true,\"authenticated\":") +
        (adminModule.isAuthenticated() ? "true" : "false") +
        ",\"remainingAttempts\":" + std::to_string(remainingAttempts) +
        ",\"locked\":" + (remainingAttempts == 0 ? "true" : "false") + "}";
    sendResponse(client, 200, "OK", "application/json", body);
}

// Verify administrator credentials through the existing AdminModule.
void handleAdminLogin(int client, const std::string& body,
                      AdminModule& adminModule, int& failedLoginAttempts) {
    std::map<std::string, std::string> values;
    if (!parseJsonObject(body, values)) {
        sendJsonError(client, 400, "Bad Request",
                      "Request body must be a valid JSON object.");
        return;
    }

    const auto username = values.find("username");
    const auto password = values.find("password");
    if (username == values.end() || password == values.end() ||
        username->second.empty() || password->second.empty()) {
        sendJsonError(client, 400, "Bad Request",
                      "Both username and password are required.");
        return;
    }

    if (failedLoginAttempts >= AdminModule::kMaxLoginAttempts) {
        const std::string response =
            "{\"success\":false,\"authenticated\":false,"
            "\"remainingAttempts\":0,"
            "\"message\":\"Maximum login attempts reached. Restart the server to try again.\"}";
        sendResponse(client, 429, "Too Many Requests", "application/json", response);
        return;
    }

    const Status result = adminModule.login(username->second, password->second);
    if (result == Status::OK) {
        failedLoginAttempts = 0;
        const std::string response =
            "{\"success\":true,\"authenticated\":true,"
            "\"remainingAttempts\":" +
            std::to_string(AdminModule::kMaxLoginAttempts) +
            ",\"message\":\"Administrator login successful.\"}";
        sendResponse(client, 200, "OK", "application/json", response);
        std::cout << "Administrator logged in through the web interface.\n";
        return;
    }

    ++failedLoginAttempts;
    const int remainingAttempts =
        std::max(0, AdminModule::kMaxLoginAttempts - failedLoginAttempts);
    const std::string message = remainingAttempts == 0
        ? "Maximum login attempts reached. Restart the server to try again."
        : "Invalid administrator username or password.";
    std::ostringstream response;
    response << "{\"success\":false,\"authenticated\":false,"
             << "\"remainingAttempts\":" << remainingAttempts << ','
             << "\"message\":" << jsonString(message) << '}';
    sendResponse(client, remainingAttempts == 0 ? 429 : 401,
                 remainingAttempts == 0 ? "Too Many Requests" : "Unauthorized",
                 "application/json", response.str());
}

// End the current administrator session.
void handleAdminLogout(int client, AdminModule& adminModule,
                       int& failedLoginAttempts) {
    adminModule.logout();
    failedLoginAttempts = 0;
    const std::string body =
        "{\"success\":true,\"authenticated\":false,"
        "\"message\":\"Administrator logged out successfully.\"}";
    sendResponse(client, 200, "OK", "application/json", body);
    std::cout << "Administrator logged out of the web interface.\n";
}

// Convert a booking to the compact row format used by the admin dashboard.
std::string adminBookingToJson(const Booking& booking) {
    std::ostringstream output;
    output << "{"
           << "\"pnr\":" << jsonString(booking.pnr) << ','
           << "\"trainNo\":" << booking.trainNo << ','
           << "\"passengerName\":" << jsonString(booking.passenger.name) << ','
           << "\"seatNo\":" << booking.seatNo << ','
           << "\"fare\":" << booking.fare << ','
           << "\"bookingDate\":" << jsonString(booking.bookingDate) << ','
           << "\"status\":" << jsonString(booking.status) << ','
           << "\"refund\":" << booking.refund
           << '}';
    return output.str();
}

bool parseNonNegativeDouble(const std::string& text, double& value) {
    if (text.empty()) return false;
    try {
        std::size_t used = 0;
        const double parsed = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(parsed) || parsed < 0.0) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

void sendAdminOperationError(int client, Status result,
                             const std::string& operation) {
    switch (result) {
        case Status::NOT_AUTHENTICATED:
            sendJsonError(client, 401, "Unauthorized",
                          "Please log in as an administrator first.");
            return;
        case Status::INVALID_INPUT:
            sendJsonError(client, 400, "Bad Request",
                          "Please check the train details and try again.");
            return;
        case Status::DUPLICATE_TRAIN_NO:
            sendJsonError(client, 409, "Conflict",
                          "A train with this number already exists.");
            return;
        case Status::TRAIN_NOT_FOUND:
            sendJsonError(client, 404, "Not Found",
                          "The requested train was not found.");
            return;
        case Status::INVALID_CAPACITY:
            sendJsonError(client, 409, "Conflict",
                          "Total seats cannot be lower than confirmed bookings.");
            return;
        case Status::TRAIN_HAS_ACTIVE_BOOKINGS:
            sendJsonError(client, 409, "Conflict",
                          "This train cannot be deleted while it has confirmed bookings.");
            return;
        case Status::FILE_ERROR:
            sendJsonError(client, 500, "Internal Server Error",
                          "The change could not be saved to the data file.");
            return;
        default:
            sendJsonError(client, 400, "Bad Request",
                          operation + " could not be completed.");
            return;
    }
}

void handleAdminTrains(int client, const AdminModule& adminModule) {
    std::vector<Train> records;
    const Status result = adminModule.viewTrains(records);
    if (result != Status::OK) {
        sendAdminOperationError(client, result, "Loading trains");
        return;
    }

    std::ostringstream body;
    body << "{\"success\":true,\"trains\":[";
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i != 0) body << ',';
        body << trainToJson(records[i]);
    }
    body << "]}";
    sendResponse(client, 200, "OK", "application/json", body.str());
}

void handleAdminBookings(int client, const AdminModule& adminModule) {
    std::vector<Booking> records;
    const Status result = adminModule.viewBookings(records);
    if (result != Status::OK) {
        sendAdminOperationError(client, result, "Loading bookings");
        return;
    }

    std::ostringstream body;
    body << "{\"success\":true,\"bookings\":[";
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i != 0) body << ',';
        body << adminBookingToJson(records[i]);
    }
    body << "]}";
    sendResponse(client, 200, "OK", "application/json", body.str());
}

void handleAdminTrainSave(int client, const std::string& body,
                          AdminModule& adminModule, bool modifying) {
    std::map<std::string, std::string> values;
    if (!parseJsonObject(body, values)) {
        sendJsonError(client, 400, "Bad Request",
                      "Request body must be a valid JSON object.");
        return;
    }

    const auto trainNoField = values.find("trainNo");
    const auto nameField = values.find("name");
    const auto sourceField = values.find("source");
    const auto destinationField = values.find("destination");
    const auto departureField = values.find("departure");
    const auto arrivalField = values.find("arrival");
    const auto fareField = values.find("fare");
    const auto seatsField = values.find("totalSeats");

    if (trainNoField == values.end() || nameField == values.end() ||
        sourceField == values.end() || destinationField == values.end() ||
        departureField == values.end() || arrivalField == values.end() ||
        fareField == values.end() || seatsField == values.end()) {
        sendJsonError(client, 400, "Bad Request",
                      "Required fields: trainNo, name, source, destination, departure, arrival, fare and totalSeats.");
        return;
    }

    Train train{};
    if (!parsePositiveInt(trainNoField->second, train.trainNo) ||
        !parsePositiveInt(seatsField->second, train.totalSeats) ||
        !parseNonNegativeDouble(fareField->second, train.fare)) {
        sendJsonError(client, 400, "Bad Request",
                      "Train number, fare or seat capacity is invalid.");
        return;
    }

    train.name = nameField->second;
    train.source = sourceField->second;
    train.destination = destinationField->second;
    train.departure = departureField->second;
    train.arrival = arrivalField->second;
    train.availableSeats = train.totalSeats;

    const Status result = modifying
        ? adminModule.modifyTrain(train.trainNo, train)
        : adminModule.addTrain(train);
    if (result != Status::OK) {
        sendAdminOperationError(client, result,
                                modifying ? "Updating train" : "Adding train");
        return;
    }

    const std::string message = modifying
        ? "Train details updated successfully."
        : "Train added successfully.";
    sendResponse(client, 200, "OK", "application/json",
        "{\"success\":true,\"message\":" + jsonString(message) + "}");
}

void handleAdminTrainDelete(int client, const std::string& body,
                            AdminModule& adminModule) {
    std::map<std::string, std::string> values;
    if (!parseJsonObject(body, values)) {
        sendJsonError(client, 400, "Bad Request",
                      "Request body must be a valid JSON object.");
        return;
    }

    const auto trainNoField = values.find("trainNo");
    int trainNo = 0;
    if (trainNoField == values.end() ||
        !parsePositiveInt(trainNoField->second, trainNo)) {
        sendJsonError(client, 400, "Bad Request",
                      "A valid train number is required.");
        return;
    }

    const Status result = adminModule.deleteTrain(trainNo);
    if (result != Status::OK) {
        sendAdminOperationError(client, result, "Deleting train");
        return;
    }

    sendResponse(client, 200, "OK", "application/json",
        "{\"success\":true,\"message\":\"Train deleted successfully.\"}");
}


bool readWebFile(const std::string& path, std::string& contents,
                 std::string& contentType) {
    std::string filePath;
    if (path == "/" || path == "/index.html") {
        filePath = "web/index.html";
        contentType = "text/html";
    } else if (path == "/style.css") {
        filePath = "web/style.css";
        contentType = "text/css";
    } else if (path == "/app.js") {
        filePath = "web/app.js";
        contentType = "application/javascript";
    } else if (path == "/admin.css") {
        filePath = "web/admin.css";
        contentType = "text/css";
    } else {
        return false;
    }

    std::ifstream input(filePath, std::ios::binary);
    if (!input) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    contents = buffer.str();
    return true;
}

void handleClient(int client, BookingModule& bookingModule,
                  TicketModule& tickets, TrainModule& trains,
                  AdminModule& adminModule, int& failedLoginAttempts) {
    HttpRequest request;
    if (!receiveRequest(client, request)) {
        sendJsonError(client, 400, "Bad Request", "Could not read the HTTP request.");
        return;
    }

    if (request.method == "GET" && request.path == "/api/trains") {
        handleTrains(client, trains);
        return;
    }

    if (request.method == "GET" && request.path == "/api/admin/session") {
        handleAdminSession(client, adminModule, failedLoginAttempts);
        return;
    }

    if (request.method == "POST" && request.path == "/api/admin/login") {
        handleAdminLogin(client, request.body, adminModule,
                         failedLoginAttempts);
        return;
    }

    if (request.method == "POST" && request.path == "/api/admin/logout") {
        handleAdminLogout(client, adminModule, failedLoginAttempts);
        return;
    }

    // Administrator management endpoints. AdminModule enforces authentication.
    if (request.method == "GET" && request.path == "/api/admin/trains") {
        handleAdminTrains(client, adminModule);
        return;
    }

    if (request.method == "GET" && request.path == "/api/admin/bookings") {
        handleAdminBookings(client, adminModule);
        return;
    }

    if (request.method == "POST" && request.path == "/api/admin/trains/add") {
        handleAdminTrainSave(client, request.body, adminModule, false);
        return;
    }

    if (request.method == "POST" && request.path == "/api/admin/trains/modify") {
        handleAdminTrainSave(client, request.body, adminModule, true);
        return;
    }

    if (request.method == "POST" && request.path == "/api/admin/trains/delete") {
        handleAdminTrainDelete(client, request.body, adminModule);
        return;
    }

    // B19: GET /api/bookings/<PNR> returns the booking details.
    const std::string bookingPrefix = "/api/bookings/";
    if (request.method == "GET" &&
        request.path.compare(0, bookingPrefix.size(), bookingPrefix) == 0) {
        const std::string pnr = request.path.substr(bookingPrefix.size());
        if (pnr.empty() || pnr.find('/') != std::string::npos) {
            sendJsonError(client, 400, "Bad Request", "Invalid booking URL.");
            return;
        }
        handleBookingLookup(client, pnr, tickets, trains);
        return;
    }

    // B19: POST /api/bookings/cancel cancels a confirmed booking.
    if (request.method == "POST" &&
        request.path == "/api/bookings/cancel") {
        handleCancellation(client, request.body, bookingModule,
                           tickets, trains);
        return;
    }

    if (request.method == "POST" && request.path == "/api/bookings") {
        handleBooking(client, request.body, bookingModule, trains);
        return;
    }
    if (request.method == "GET") {
        std::string contents;
        std::string contentType;
        if (readWebFile(request.path, contents, contentType)) {
            sendResponse(client, 200, "OK", contentType, contents);
        } else {
            sendJsonError(client, 404, "Not Found", "Page not found.");
        }
        return;
    }
    sendJsonError(client, 405, "Method Not Allowed", "This HTTP method is not supported for that path.");
}

}  // namespace

int main(int argc, char* argv[]) {
    int port = kDefaultPort;
    if (argc > 1) {
        try {
            const int requested = std::stoi(argv[1]);
            if (requested < 1024 || requested > 65535) {
                std::cerr << "Port must be between 1024 and 65535.\n";
                return 1;
            }
            port = requested;
        } catch (...) {
            std::cerr << "Usage: ./booking_server [port]\n";
            return 1;
        }
    }

    std::signal(SIGPIPE, SIG_IGN);

    FileModule files;
    std::vector<Train> loadedTrains;
    std::vector<Booking> bookings;
    const LoadReport report = files.loadAll(loadedTrains, bookings);
    if (loadedTrains.empty()) {
        std::cerr << "No train records were loaded. Run the server from the project root, "
                     "where trains.txt and bookings.txt are located.\n";
        return 1;
    }

    TrainModule trains(std::move(loadedTrains));
    ValidationModule validation;
    FareModule fares(trains);
    TicketModule tickets(bookings);
    BookingModule bookingModule(trains, bookings, validation, fares, tickets, files);
    AdminModule adminModule(trains, bookings, files);
    int failedAdminLoginAttempts = 0;

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Could not create server socket.\n";
        return 1;
    }

    int reuseAddress = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
               &reuseAddress, sizeof(reuseAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0) {
        std::cerr << "Could not bind to 127.0.0.1:" << port
                  << ". Is another server already using this port?\n";
        close(serverSocket);
        return 1;
    }
    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Could not start listening on the server socket.\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Railway booking server started at http://127.0.0.1:"
              << port << "\n"
              << "Loaded " << report.trainsLoaded << " trains and "
              << report.bookingsLoaded << " bookings.\n"
              << "Open the URL in your browser. Press Ctrl+C to stop.\n";

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        const int client = accept(serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (client < 0) continue;
        handleClient(client, bookingModule, tickets, trains,
                     adminModule, failedAdminLoginAttempts);
        close(client);
    }

    close(serverSocket);
    return 0;
}
