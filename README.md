# Railway Ticket Reservation System

**Software Engineering Mini-Project · Group 15**

A console-based railway ticket reservation application developed in **C++17**, designed to support train search, ticket booking, reservation management, ticket cancellation, and administrative operations.

---

## Overview

The Railway Ticket Reservation System aims to simplify railway reservation operations through a menu-driven console application. It enables passengers to search for trains, check seat availability, book tickets, retrieve booking details using a PNR, and cancel existing reservations.

The system also provides administrative functionality for managing train records and viewing train and booking information.

The application is designed as a standalone system using local text files for persistent data storage.

## Objectives

- Display available trains and support searching by source, destination, or train number.
- Show train details and seat availability.
- Validate passenger information before booking.
- Generate unique Passenger Name Record (PNR) identifiers and calculate fares.
- Support booking retrieval, ticket cancellation, and refund calculation.
- Maintain consistent seat availability and persistent booking records.
- Provide authenticated administrative operations for train management.
- Follow a modular, layered architecture to improve maintainability and testability.


## Team Members

| Team Member | Role | SRN |
|---|---|---|
| Monisha Agarwal | Developer | `PES2UG24CS285` |
| Lakshmi Pranathi Tummalapenta | Scrum Master | `PES2UG24CS249` |
| Mohammed Farhan Ahmed | Developer | `PES2UG24CS280` |
| Nandita N Patil | Developer | `PES2UG24CS304` |


### Sprint Schedule

| Sprint | Dates | Focus |
|---|---|---|
| Sprint 1 | 19–23 October 2026 | Initial implementation and working product |
| Sprint 2 | 26–30 October 2026 | Remaining features, integration, testing, and documentation |

## Architecture

The Railway Reservation System follows a three-layer architecture defined in the Software Architecture and Design Specification.

- **Presentation Layer:** `UserInterface` handles menus, prompts, confirmations, and messages.
- **Business Logic Layer:** `ValidationModule`, `TrainModule`, `SearchModule`, `SeatModule`, `FareModule`, `TicketModule`, `BookingModule`, `CancellationModule`, `RefundModule`, and `AdminModule`.
- **Data Access Layer:** `FileModule` is the only module responsible for persistent file access.

## Project Structure

- `include/` — Header files, public interfaces, and shared data types.
- `src/` — Module implementations and the console application entry point.
- `server/` — Small HTTP server that exposes the existing C++ booking modules to the web interface.
- - `tests/` — Module-level, acceptance, and non-functional test cases.
- `.github/workflows/` — Automated build checks.

## Build

Compile with a C++17-compatible compiler:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o railway
```

Run the setup application:

```bash
./railway
```

## Web Interface (B19)

The browser interface uses the same C++ modules and local `trains.txt` and
`bookings.txt` files as the console application. Build and start the server
from the project root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude server/booking_server.cpp src/AdminModule.cpp src/BookingModule.cpp src/FareModule.cpp src/FileModule.cpp src/TicketModule.cpp src/TrainModule.cpp src/ValidationModule.cpp -o booking_server
./booking_server
```

Open **http://127.0.0.1:8080** in a browser. Keep the server terminal running
while using the website. Press `Ctrl+C` to stop it. The server binds to the
loopback address and is intended for local coursework use; do not expose this
simple development server to an untrusted network.

The web API provides:

- `GET /api/trains` — returns trains, fares, and current available seats.
- `POST /api/bookings` — validates passenger details and creates a booking.
- `GET /api/bookings/{PNR}` — retrieves a booking for passenger review.
- `POST /api/bookings/cancel` — cancels a confirmed booking and returns the
  updated booking details, including the stored refund amount.
- `POST /api/admin/login` — checks credentials through `AdminModule`.
- `GET /api/admin/session` — reports the current in-memory administrator login
  state and remaining login attempts.
- `POST /api/admin/logout` — ends the administrator session.

The administrator login uses the existing credentials configured in
`AdminModule`; credentials are checked by the C++ backend and are not stored
in browser local storage. Three failed login attempts lock the web login until
the server is restarted. The web page currently provides administrator login,
session status, and logout; train-management operations continue to be
available through the console administrator menu.

Bookings and cancellations made through the website update the project's
local data files. Do not open `web/index.html` directly with `file://`; use the
local server URL so the browser can reach the API.


## Administrator Access

Choose **8. Administrator login** from the main menu and enter the administrator credentials:

| Username | Password |
|---|---|
| `admin` | `Admin@123` |

Three failed attempts return you to the main menu. Administrative operations are refused unless an administrator is logged in, even when called directly from code. Leaving the administrator menu logs the administrator out.

After login, choose **1. Add train** to enter a new train record or **2. Modify train** to update an existing record. Train numbers are unique and remain fixed when editing; train capacity cannot be reduced below the number of confirmed bookings. Choose **3. View all trains** or **4. View all bookings** to inspect the current records. Choose **5. Delete train** and confirm the prompt to remove an eligible train. A train with any confirmed bookings cannot be deleted; canceled bookings do not prevent deletion. Train changes are saved to `trains.txt`, while booking records and their original fares remain stored in `bookings.txt`.

Run the administrator tests with:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude tests/test_admin.cpp src/AdminModule.cpp src/TrainModule.cpp src/FileModule.cpp -o test_admin && ./test_admin
```

Run the B12 add/modify acceptance tests with:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude tests/test_admin_trains.cpp src/AdminModule.cpp src/TrainModule.cpp src/FileModule.cpp -o test_admin_trains && ./test_admin_trains
```

## Non-Functional Tests

Run the B14 performance, seat consistency and usability tests (see `tests/NFR_TEST_REPORT.md` for results):

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude tests/test_nfr_performance.cpp src/BookingModule.cpp src/FareModule.cpp src/FileModule.cpp src/SearchModule.cpp src/TicketModule.cpp src/TrainModule.cpp src/ValidationModule.cpp -o test_nfr_performance && ./test_nfr_performance
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude tests/test_nfr_seats.cpp src/BookingModule.cpp src/FareModule.cpp src/FileModule.cpp src/TicketModule.cpp src/TrainModule.cpp src/ValidationModule.cpp -o test_nfr_seats && ./test_nfr_seats
g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o railway && tests/test_nfr_usability.sh ./railway
```
