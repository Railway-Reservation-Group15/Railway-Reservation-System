// Railway Reservation System - browser interface.
// Train data and booking actions are served by the C++ backend.

let trains = [];

// Train search elements
const searchForm = document.querySelector("#train-search");
const sourceInput = document.querySelector("#source");
const destinationInput = document.querySelector("#destination");
const results = document.querySelector("#train-results");
const resultsTitle = document.querySelector("#results-title");
const resultsSummary = document.querySelector("#results-summary");

// Booking elements
const bookingForm = document.querySelector("#booking-form");
const bookingMessage = document.querySelector("#booking-message");
const bookingConfirmation = document.querySelector("#booking-confirmation");
const bookingSubmitButton = bookingForm.querySelector('button[type="submit"]');
const trainNumberInput = document.querySelector("#booking-train-number");
const trainNumberList = document.querySelector("#booking-train-list");

function formatFare(amount) {
  return `₹${Number(amount).toLocaleString("en-IN", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  })}`;
}

function createResult(train) {
  const card = document.createElement("article");
  card.className = "train-result";

  const identity = document.createElement("div");
  const name = document.createElement("span");
  name.className = "train-name";
  name.textContent = train.name;

  const number = document.createElement("span");
  number.className = "train-number";
  number.textContent = `Train ${train.trainNo}`;
  identity.append(name, number);

  const route = document.createElement("div");
  route.className = "route-details";

  const times = document.createElement("span");
  times.className = "route-time";
  times.textContent = `${train.departure}  →  ${train.arrival}`;

  const stations = document.createElement("span");
  stations.className = "route-stations";
  stations.textContent = `${train.source}  →  ${train.destination}`;
  route.append(times, stations);

  const fare = document.createElement("div");
  const fareLabel = document.createElement("span");
  fareLabel.className = "result-label";
  fareLabel.textContent = "Fare";

  const fareValue = document.createElement("span");
  fareValue.className = "train-fare";
  fareValue.textContent = formatFare(train.fare);
  fare.append(fareLabel, fareValue);

  const availability = document.createElement("div");
  const seatLabel = document.createElement("span");
  seatLabel.className = "result-label";
  seatLabel.textContent = "Seats available";

  const seatValue = document.createElement("span");
  seatValue.className = `seat-count${train.availableSeats === 0 ? " sold-out" : ""}`;
  seatValue.textContent = train.availableSeats === 0
    ? "Sold out"
    : String(train.availableSeats);
  availability.append(seatLabel, seatValue);

  card.append(identity, route, fare, availability);
  return card;
}

function showTrains(matches, searching) {
  results.replaceChildren();
  resultsTitle.textContent = searching ? "Search results" : "Available trains";
  resultsSummary.textContent = searching
    ? `${matches.length} ${matches.length === 1 ? "train" : "trains"} found`
    : "Live timetable and seat availability from the C++ backend";

  if (matches.length === 0) {
    const message = document.createElement("p");
    message.className = "search-message";
    message.textContent = searching
      ? "No trains found for that route. Check the station names and try again."
      : "No train records are currently available.";
    results.append(message);
    return;
  }

  results.append(...matches.map(createResult));
}

function populateTrainChoices() {
  trainNumberList.replaceChildren();
  trains.forEach((train) => {
    const option = document.createElement("option");
    option.value = String(train.trainNo);
    option.label = `${train.name} (${train.source} to ${train.destination})`;
    trainNumberList.append(option);
  });
}

async function loadTrains(showError = true) {
  try {
    const response = await fetch("/api/trains");
    const data = await response.json();
    if (!response.ok || !data.success || !Array.isArray(data.trains)) {
      throw new Error(data.message || "Could not load train records.");
    }

    trains = data.trains;
    populateTrainChoices();
    showTrains(trains, false);
    return true;
  } catch (error) {
    trains = [];
    populateTrainChoices();
    showTrains([], false);
    resultsSummary.textContent = "C++ backend unavailable";
    if (showError) {
      bookingMessage.textContent =
        "Cannot connect to the C++ backend. Start the booking server from the project folder, then refresh this page.";
    }
    console.error("Could not load trains:", error);
    return false;
  }
}

// Search the live train records returned by the C++ backend.
searchForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const source = sourceInput.value.trim().toLocaleLowerCase();
  const destination = destinationInput.value.trim().toLocaleLowerCase();

  const matches = trains.filter((train) =>
    train.source.toLocaleLowerCase() === source &&
    train.destination.toLocaleLowerCase() === destination
  );
  showTrains(matches, true);
});

function displayBooking(booking) {
  document.querySelector("#confirmation-pnr").textContent = booking.pnr;
  document.querySelector("#confirmation-train").textContent =
    `${booking.trainNo} - ${booking.trainName} (${booking.source} → ${booking.destination}), ${booking.departure}–${booking.arrival}`;
  document.querySelector("#confirmation-passenger").textContent = booking.passenger.name;
  document.querySelector("#confirmation-age").textContent = String(booking.passenger.age);
  const genderLabels = { M: "Male", F: "Female", O: "Other" };
  document.querySelector("#confirmation-gender").textContent =
    genderLabels[booking.passenger.gender] || booking.passenger.gender;
  document.querySelector("#confirmation-contact").textContent = booking.passenger.contact;
  document.querySelector("#confirmation-seat").textContent = String(booking.seatNo);
  document.querySelector("#confirmation-fare").textContent = formatFare(booking.fare);
  document.querySelector("#confirmation-date").textContent = booking.bookingDate;
  document.querySelector("#confirmation-status").textContent = booking.status;

  bookingConfirmation.hidden = false;
  bookingConfirmation.scrollIntoView({ behavior: "smooth", block: "nearest" });
}

bookingForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  bookingConfirmation.hidden = true;
  bookingMessage.textContent = "";

  const payload = {
    trainNo: Number(trainNumberInput.value),
    name: document.querySelector("#passenger-name").value.trim(),
    age: Number(document.querySelector("#passenger-age").value),
    gender: document.querySelector("#passenger-gender").value,
    contact: document.querySelector("#passenger-contact").value.trim()
  };

  // Match the important passenger rules enforced by ValidationModule.cpp.
  const validName = payload.name.length <= 50 &&
    /^[A-Za-z][A-Za-z .'-]*$/.test(payload.name) &&
    /[A-Za-z.]$/.test(payload.name);
  if (!validName) {
    bookingMessage.textContent =
      "Enter a valid name (up to 50 characters; letters, spaces, apostrophes, hyphens and periods only).";
    return;
  }
  if (!Number.isInteger(payload.age) || payload.age < 1 || payload.age > 120) {
    bookingMessage.textContent = "Enter an age between 1 and 120.";
    return;
  }
  if (!/^[6-9][0-9]{9}$/.test(payload.contact)) {
    bookingMessage.textContent =
      "Enter a valid 10-digit Indian mobile number starting with 6, 7, 8 or 9.";
    return;
  }
  if (!trains.some((train) => train.trainNo === payload.trainNo)) {
    bookingMessage.textContent = "Select a train number from the available timetable.";
    return;
  }

  bookingSubmitButton.disabled = true;
  bookingSubmitButton.textContent = "Booking...";
  bookingMessage.textContent = "Sending booking to the C++ backend...";

  try {
    const response = await fetch("/api/bookings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    const data = await response.json();
    if (!response.ok || !data.success || !data.booking) {
      throw new Error(data.message || "Booking could not be completed.");
    }

    displayBooking(data.booking);
    bookingMessage.textContent =
      `Booking confirmed. PNR ${data.booking.pnr} has been saved to bookings.txt.`;
    await loadTrains(false);
  } catch (error) {
    bookingMessage.textContent = error.message ||
      "Could not reach the C++ booking server. Check that it is running and try again.";
    console.error("Booking request failed:", error);
  } finally {
    bookingSubmitButton.disabled = false;
    bookingSubmitButton.textContent = "Book Ticket";
  }
});

// Load authoritative train and seat data as soon as the page opens.
loadTrains();
