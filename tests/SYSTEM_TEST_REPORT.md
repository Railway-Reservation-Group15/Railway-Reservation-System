# System Test Report: Railway Ticket Reservation System

**Project:** Railway Ticket Reservation System, Group 15
**Build tested (commit hash):** 2ebd7e2
**Execution date(s):** 2 November 2026
**Environment:** Windows with WSL (Ubuntu 22.04) · g++ 11.4.0 


## 1. Automated Test Execution

| ID | Suite | Command / Source | Result | Notes |
|---|---|---|---|---|
| AT-01 | Application build with warnings on | `g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o railway` | Pass | No warnings or errors |
| AT-02 | ValidationModule | `tests/test_validation.cpp` | Pass | All ValidationModule tests passed |
| AT-03 | BookingModule (booking, retrieval, cancellation, refund) | `tests/test_booking.cpp` | Pass | B07, B08, B09 and B10 checks passed |
| AT-04 | AdminModule (login, session) | `tests/test_admin.cpp` | Pass | All AdminModule tests passed |
| AT-05 | Admin train add/modify | `tests/test_admin_trains.cpp` | Pass | All B12 tests passed |
| AT-06 | TC-NFR-01 Search performance (10,000 trains, 2000 ms limit) | `tests/test_nfr_performance.cpp` | Pass | Slowest route search 6.256 ms; 100 searches 614.131 ms (limit 2000 ms); load 34.911 ms; booking 1.069 ms |
| AT-07 | TC-NFR-02 Seat consistency | `tests/test_nfr_seats.cpp` | Pass | All seat consistency checks passed |
| AT-08 | TC-NFR-03 Menu usability | `tests/test_nfr_usability.sh` | Pass | All 30 checks passed |
| AT-09 | GitHub Actions CI run on `main` | Actions tab | Pass | Build and automated tests executed successfully through GitHub Actions |

## 2. Manual Test Cases and Execution Results

### Passenger functions (console unless stated)

| ID | Feature | Steps | Expected Result | Actual Result | Status |
|---|---|---|---|---|---|
| TC-01 | View all trains | Main menu, choose View all trains | All trains from `trains.txt` listed with number, name, route, times, fare | Matched expected result. | Pass |
| TC-02 | Search by route | Search with source `Bengaluru`, destination `Chennai` | Trains 12028, 12608 and 12657 listed | Matched expected result. | Pass |
| TC-03 | Search, case and spacing | Search source ` bengaluru ` (lower case, extra spaces), destination blank | Same results as upper/proper case; blank means any | Matched expected result. | Pass |
| TC-04 | Search by train number | Enter `12627` | Karnataka Express details shown | Matched expected result. | Pass |
| TC-05 | Search, no match | Enter train number `99999` | Clear "not found" message; app does not crash | Matched expected result. | Pass |
| TC-06 | Seat availability | View details for `16217` | Available seats = total seats minus confirmed bookings | Matched expected result. | Pass |
| TC-07 | Book a ticket, valid | Train `16217`, name `Test User`, age `30`, gender `F`, mobile `9123456780` | Confirmation with 10-digit PNR, seat number, fare Rs. 210, status CONFIRMED; record appended to `bookings.txt` | PNR, seat and fare Rs. 210 shown; status CONFIRMED. | Pass |
| TC-08 | Seat decreases after booking | Repeat TC-06 after TC-07 | Available seats reduced by 1 | Matched expected result. | Pass |
| TC-09 | Book, invalid age | Age `0`, then `121`, then `abc` | Each rejected with an explanatory message; no booking created | Matched expected result. | Pass |
| TC-10 | Book, invalid mobile | Mobile `12345`, then `98765abc10` | Rejected; message states 10 digits needed | Matched expected result. | Pass |
| TC-11 | Book, invalid name | Empty name; name longer than 50 characters | Rejected; no booking created | Matched expected result. | Pass |
| TC-12 | Book, invalid gender | Gender `X` | Rejected; allowed values shown | Matched expected result. | Pass |
| TC-13 | Book, unknown train | Train `99999` | "Train not found" style message; no booking created | Matched expected result. | Pass |
| TC-14 | Book, train full | Train `17301` (5 of 5 seats booked) | "No seats are available" message; no booking created | "No seats are available" shown; no booking created. | Pass |
| TC-15 | Retrieve by PNR | Use PNR from TC-07 | Same passenger, train, seat, fare and status shown | Matched expected result. | Pass |
| TC-16 | Retrieve, bad PNR | `123`, then `0000000000` | Malformed PNR explains the expected format; unknown PNR says not found | Matched expected result. | Pass |
| TC-17 | Cancel, valid | Cancel PNR from TC-07 after confirming the prompt | Status CANCELLED; refund Rs. 189.00 (90% of 210); charge Rs. 21.00 | Refund Rs. 189.00 and charge Rs. 21.00. | Pass |
| TC-18 | Cancel, seat released | Repeat TC-06 after TC-17 | Available seats back to the pre-TC-07 count | Matched expected result. | Pass |
| TC-19 | Cancel twice | Cancel the same PNR again | Refused: already cancelled; refund not paid twice | Refused as already cancelled. | Pass |
| TC-20 | Cancel, declined prompt | Start a cancel, answer No at the confirmation | Booking stays CONFIRMED | Matched expected result. | Pass |
| TC-21 | Persistence | Book, exit the app, restart it, retrieve the PNR | Booking still present | Matched expected result. | Pass |
| TC-22 | Invalid menu input | Enter `abc`, then `99` | Explained; valid options listed; app keeps running | Matched expected result. | Pass |

### Administrator functions

| ID | Feature | Steps | Expected Result | Actual Result | Status |
|---|---|---|---|---|---|
| TC-23 | Admin login, valid | `admin` / `Admin@123` | Admin menu opens | Matched expected result. | Pass |
| TC-24 | Admin login, invalid | Wrong password once | Rejected; remaining attempts decrease | Matched expected result. | Pass |
| TC-25 | Admin lockout | Three wrong attempts (console) | Returned to main menu | Matched expected result. | Pass |
| TC-26 | Admin protection | Call `GET /api/admin/bookings` without logging in | `success:false`, "log in as an administrator first" | Matched expected result. | Pass |
| TC-27 | Add train | Add train `10001`, any valid details | Saved to `trains.txt`; visible in View all trains | Matched expected result. | Pass |
| TC-28 | Add train, duplicate number | Add `12627` again | Rejected; train numbers are unique | Matched expected result. | Pass |
| TC-29 | Modify train | Change fare of `10001` | Updated; train number unchanged | Matched expected result. | Pass |
| TC-30 | Modify, capacity below bookings | Reduce capacity of `16217` below its confirmed bookings | Rejected | Matched expected result. | Pass |
| TC-31 | Delete train, no bookings | Delete `10001` and confirm | Removed from `trains.txt` | Matched expected result. | Pass |
| TC-32 | Delete train, has bookings | Delete `12627` | Refused: confirmed bookings exist | Refused: confirmed bookings exist. | Pass |
| TC-33 | View all bookings | Admin menu, View all bookings | All records shown, including cancelled ones with refund | Matched expected result. | Pass |
| TC-34 | Logout | Leave admin menu / press Logout on site | Admin operations refused again | Matched expected result. | Pass |

### Website (run `./booking_server`, open http://127.0.0.1:8080)

| ID | Feature | Steps | Expected Result | Actual Result | Status |
|---|---|---|---|---|---|
| TC-35 | Page loads | Open the URL | Train list and search load; no console errors in browser dev tools | Matched expected result. | Pass |
| TC-36 | Web booking | Book on train `16217` through the form | Confirmation with PNR, seat, fare | Matched expected result. | Pass |
| TC-37 | Web and console share data | Retrieve the TC-36 PNR in the console app | Booking found | Matched expected result. | Pass |
| TC-38 | Web form validation | Submit with empty fields, age `-5` | Rejected with a message; no booking created | Matched expected result. | Pass |
| TC-39 | Web cancel | Cancel the TC-36 booking | Shows refund Rs. 189.00 and charge Rs. 21.00 | Refund Rs. 189.00 and charge Rs. 21.00. | Pass |
| TC-40 | Web admin login lockout | Three wrong logins | Login locked until server restart | Matched expected result. | Pass |
| TC-41 | Web admin management | Add, modify, delete a train; view bookings | Same rules as TC-27 to TC-33 | Matched expected result. | Pass |
| TC-42 | Malformed API request | `curl -X POST localhost:8080/api/bookings -d 'garbage'` | `success:false`, "valid JSON object"; server keeps running | "Request body must be a valid JSON object"; server kept running. | Pass |


## 3. Test Summary

| Metric | Count |
|---|---:|
| Application build checks passed | 1 |
| Automated test suites run | 7 |
| Automated test suites passed | 7 |
| GitHub Actions CI | Pass |
| Manual cases planned | 42 |
| Manual cases executed | 42 |
| Passed | 42 |
| Failed | 0 |
| Blocked / not run | 0 |
| Pass rate (Passed ÷ Executed) | 100% |

**Coverage:** passenger search, booking, retrieval, cancellation and refund; administrator login and train management; persistence; website and API; performance, seat consistency and usability.
