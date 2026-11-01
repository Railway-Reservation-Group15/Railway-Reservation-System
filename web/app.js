// Railway Reservation System - browser interface.
// Train data and booking actions are served by the C++ backend.

let trains = [];
let currentCancellationBooking = null;

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

// Cancellation elements
const cancellationLookupForm = document.querySelector("#cancellation-lookup-form");
const cancellationPnrInput = document.querySelector("#cancel-pnr");
const cancellationMessage = document.querySelector("#cancellation-message");
const cancellationDetails = document.querySelector("#cancellation-details");
const confirmCancellationButton = document.querySelector("#confirm-cancellation-button");
const cancellationResult = document.querySelector("#cancellation-result");

// Administrator login elements
const adminLoginForm = document.querySelector("#admin-login-form");
const adminUsernameInput = document.querySelector("#admin-username");
const adminPasswordInput = document.querySelector("#admin-password");
const adminLoginButton = document.querySelector("#admin-login-button");
const adminMessage = document.querySelector("#admin-message");
const adminSession = document.querySelector("#admin-session");
const adminSessionMessage = document.querySelector("#admin-session-message");
const adminLogoutButton = document.querySelector("#admin-logout-button");

function formatFare(amount) {
  return `₹${Number(amount).toLocaleString("en-IN", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
  })}`;
}

async function readApiResponse(response, defaultMessage) {
  let data;
  try {
    data = await response.json();
  } catch (error) {
    throw new Error(defaultMessage);
  }

  if (!response.ok || data.success === false) {
    throw new Error(data.message || defaultMessage);
  }

  return data;
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
    const data = await readApiResponse(response, "Could not load train records.");
    if (!Array.isArray(data.trains)) {
      throw new Error("The server returned invalid train data.");
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

    const data = await readApiResponse(response, "Booking could not be completed.");
    if (!data.booking) {
      throw new Error("The server did not return the new booking details.");
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

function displayCancellationBooking(booking) {
  document.querySelector("#cancel-booking-pnr").textContent = booking.pnr;
  document.querySelector("#cancel-booking-passenger").textContent = booking.passenger.name;
  document.querySelector("#cancel-booking-train").textContent =
    `${booking.trainNo} - ${booking.trainName}`;
  document.querySelector("#cancel-booking-route").textContent =
    `${booking.source} → ${booking.destination} (${booking.departure}–${booking.arrival})`;
  document.querySelector("#cancel-booking-seat").textContent = String(booking.seatNo);
  document.querySelector("#cancel-booking-date").textContent = booking.bookingDate;
  document.querySelector("#cancel-booking-fare").textContent = formatFare(booking.fare);
  document.querySelector("#cancel-booking-status").textContent = booking.status;

  const isConfirmed = String(booking.status).toUpperCase() === "CONFIRMED";
  confirmCancellationButton.disabled = !isConfirmed;
  confirmCancellationButton.textContent = isConfirmed
    ? "Confirm Cancellation"
    : "Booking Cannot Be Cancelled";

  if (!isConfirmed) {
    cancellationMessage.textContent =
      String(booking.status).toUpperCase() === "CANCELLED"
        ? "This booking has already been cancelled."
        : "Only confirmed bookings can be cancelled.";
  }

  cancellationDetails.hidden = false;
  cancellationDetails.scrollIntoView({ behavior: "smooth", block: "nearest" });
}

function displayCancellationResult(booking) {
  const fare = Number(booking.fare);
  const refund = Number(booking.refund);
  const charge = Number.isFinite(Number(booking.cancellationCharge))
    ? Number(booking.cancellationCharge)
    : fare - refund;

  document.querySelector("#cancel-result-pnr").textContent = booking.pnr;
  document.querySelector("#cancel-result-fare").textContent = formatFare(fare);
  document.querySelector("#cancel-result-charge").textContent = formatFare(charge);
  document.querySelector("#cancel-result-refund").textContent = formatFare(refund);
  document.querySelector("#cancel-result-status").textContent =
    "Cancellation completed successfully. The seat has been released.";

  cancellationResult.hidden = false;
  cancellationResult.scrollIntoView({ behavior: "smooth", block: "nearest" });
}

// Look up the booking first so the passenger can review its details.
cancellationLookupForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  cancellationMessage.textContent = "";
  cancellationDetails.hidden = true;
  cancellationResult.hidden = true;
  confirmCancellationButton.disabled = true;
  currentCancellationBooking = null;

  const pnr = cancellationPnrInput.value.trim();
  if (!/^[0-9]{10}$/.test(pnr)) {
    cancellationMessage.textContent = "Enter a valid 10-digit PNR.";
    return;
  }

  const lookupButton = cancellationLookupForm.querySelector('button[type="submit"]');
  lookupButton.disabled = true;
  lookupButton.textContent = "Searching...";
  cancellationMessage.textContent = "Retrieving booking details...";

  try {
    const response = await fetch(`/api/bookings/${encodeURIComponent(pnr)}`);
    const data = await readApiResponse(response, "Could not retrieve this booking.");
    if (!data.booking) {
      throw new Error("The server did not return booking details.");
    }

    currentCancellationBooking = data.booking;
    cancellationMessage.textContent = "Booking found. Review the details before cancelling.";
    displayCancellationBooking(data.booking);
  } catch (error) {
    cancellationMessage.textContent = error.message ||
      "Could not retrieve the booking. Check the PNR and try again.";
    console.error("Booking lookup failed:", error);
  } finally {
    lookupButton.disabled = false;
    lookupButton.textContent = "Find Booking";
  }
});

// Cancel only after the booking details have been found and reviewed.
confirmCancellationButton.addEventListener("click", async () => {
  if (!currentCancellationBooking ||
      String(currentCancellationBooking.status).toUpperCase() !== "CONFIRMED") {
    cancellationMessage.textContent = "Find a confirmed booking before cancelling it.";
    return;
  }

  const pnr = currentCancellationBooking.pnr;
  const confirmed = window.confirm(
    `Are you sure you want to cancel ticket ${pnr}? The refund will be 90% of the stored fare.`
  );
  if (!confirmed) return;

  confirmCancellationButton.disabled = true;
  confirmCancellationButton.textContent = "Cancelling...";
  cancellationMessage.textContent = "Sending cancellation request...";
  cancellationResult.hidden = true;

  try {
    const response = await fetch("/api/bookings/cancel", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ pnr })
    });

    const data = await readApiResponse(response, "The booking could not be cancelled.");
    if (!data.booking) {
      throw new Error("The server did not return the updated booking details.");
    }

    currentCancellationBooking = data.booking;
    displayCancellationBooking(data.booking);
    cancellationMessage.textContent = data.message || "Booking cancelled successfully.";
    displayCancellationResult(data.booking);
    await loadTrains(false);
  } catch (error) {
    cancellationMessage.textContent = error.message ||
      "Could not complete cancellation. Please try again.";
    console.error("Cancellation request failed:", error);
    confirmCancellationButton.disabled = false;
    confirmCancellationButton.textContent = "Confirm Cancellation";
  }
});

// Clear an old preview when the user starts entering a different PNR.
cancellationPnrInput.addEventListener("input", () => {
  currentCancellationBooking = null;
  cancellationDetails.hidden = true;
  cancellationResult.hidden = true;
  cancellationMessage.textContent = "";
  confirmCancellationButton.disabled = true;
  confirmCancellationButton.textContent = "Confirm Cancellation";
});



function showAdminSession(authenticated) {
  adminLoginForm.hidden = authenticated;
  adminSession.hidden = !authenticated;

  if (authenticated) {
    adminSessionMessage.textContent =
      "You are logged in as an administrator.";
    adminMessage.textContent = "Administrator login successful.";
  }
}

function setAdminLoginLock(locked) {
  adminUsernameInput.disabled = locked;
  adminPasswordInput.disabled = locked;
  adminLoginButton.disabled = locked;

  if (locked) {
    adminMessage.textContent =
      "Maximum login attempts reached. Restart the booking server to try again.";
  }
}

// Check whether an administrator session is already active on the server.
async function loadAdminSession() {
  try {
    const response = await fetch("/api/admin/session", { cache: "no-store" });
    const data = await readApiResponse(
      response,
      "Could not check administrator login status."
    );

    showAdminSession(Boolean(data.authenticated));
    setAdminLoginLock(Boolean(data.locked));
  } catch (error) {
    adminMessage.textContent = error.message ||
      "Could not connect to the administrator service.";
    console.error("Could not load administrator session:", error);
  }
}

adminLoginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  adminMessage.textContent = "";

  const username = adminUsernameInput.value.trim();
  const password = adminPasswordInput.value;
  if (!username || !password) {
    adminMessage.textContent = "Enter both username and password.";
    return;
  }

  adminLoginButton.disabled = true;
  adminLoginButton.textContent = "Signing in...";

  try {
    const response = await fetch("/api/admin/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username, password })
    });

    const data = await response.json();
    if (!response.ok || !data.success) {
      if (Number.isInteger(data.remainingAttempts)) {
        setAdminLoginLock(data.remainingAttempts === 0);
        if (data.remainingAttempts > 0) {
          adminMessage.textContent =
            `${data.message} ${data.remainingAttempts} attempt(s) remaining.`;
        }
      }
      if (!data.message) {
        throw new Error("Administrator login failed.");
      }
      if (data.remainingAttempts !== 0) {
        adminMessage.textContent = data.message;
      }
      return;
    }

    adminPasswordInput.value = "";
    showAdminSession(true);
    adminLoginForm.reset();
  } catch (error) {
    adminMessage.textContent = error.message ||
      "Could not complete administrator login. Please try again.";
    console.error("Administrator login failed:", error);
  } finally {
    adminLoginButton.textContent = "Login";
    if (!adminLoginButton.disabled) {
      adminLoginButton.disabled = false;
    }
  }
});

adminLogoutButton.addEventListener("click", async () => {
  adminLogoutButton.disabled = true;
  adminLogoutButton.textContent = "Logging out...";

  try {
    const response = await fetch("/api/admin/logout", {
      method: "POST"
    });
    const data = await readApiResponse(
      response,
      "Could not complete administrator logout."
    );

    showAdminSession(false);
    adminLoginForm.reset();
    setAdminLoginLock(false);
    adminMessage.textContent = data.message || "You have been logged out.";
  } catch (error) {
    adminMessage.textContent = error.message ||
      "Could not complete administrator logout. Please try again.";
    console.error("Administrator logout failed:", error);
  } finally {
    adminLogoutButton.disabled = false;
    adminLogoutButton.textContent = "Logout";
  }
});

// Load authoritative train and seat data as soon as the page opens.
loadTrains();
loadAdminSession();
