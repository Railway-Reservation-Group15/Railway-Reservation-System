// Sample timetable copied from trains.txt. Availability reflects the current
// confirmed bookings in bookings.txt and will not update without a server/API.
const trains = [
  { number: 12627, name: "Karnataka Express", from: "Bengaluru", to: "New Delhi", departure: "19:20", arrival: "06:30", fare: 1850.50, seats: 120, booked: 2 },
  { number: 12628, name: "Karnataka Express", from: "New Delhi", to: "Bengaluru", departure: "21:15", arrival: "09:00", fare: 1850.50, seats: 120, booked: 0 },
  { number: 22692, name: "Rajdhani Express", from: "Bengaluru", to: "New Delhi", departure: "20:00", arrival: "05:45", fare: 2650.00, seats: 60, booked: 2 },
  { number: 12028, name: "Shatabdi Express", from: "Bengaluru", to: "Chennai", departure: "06:00", arrival: "11:00", fare: 950.00, seats: 80, booked: 3 },
  { number: 12608, name: "Lalbagh Express", from: "Bengaluru", to: "Chennai", departure: "06:30", arrival: "12:15", fare: 780.00, seats: 100, booked: 0 },
  { number: 12657, name: "Chennai Mail", from: "Bengaluru", to: "Chennai", departure: "23:45", arrival: "05:30", fare: 520.00, seats: 110, booked: 1 },
  { number: 16217, name: "Mysuru Express", from: "Bengaluru", to: "Mysuru", departure: "07:15", arrival: "09:45", fare: 210.00, seats: 150, booked: 2 },
  { number: 12785, name: "Kacheguda Express", from: "Bengaluru", to: "Hyderabad", departure: "18:30", arrival: "07:10", fare: 890.00, seats: 100, booked: 0 },
  { number: 11301, name: "Udyan Express", from: "Bengaluru", to: "Mumbai", departure: "08:00", arrival: "11:30", fare: 1450.00, seats: 120, booked: 0 },
  { number: 17301, name: "Dharwad Express", from: "Bengaluru", to: "Hubballi", departure: "21:30", arrival: "07:00", fare: 430.00, seats: 5, booked: 5 }
];

const form = document.querySelector("#train-search");
const sourceInput = document.querySelector("#source");
const destinationInput = document.querySelector("#destination");
const results = document.querySelector("#train-results");
const resultsTitle = document.querySelector("#results-title");
const resultsSummary = document.querySelector("#results-summary");

function createResult(train) {
  const available = Math.max(0, train.seats - train.booked);
  const card = document.createElement("article");
  card.className = "train-result";

  const identity = document.createElement("div");
  const name = document.createElement("span");
  name.className = "train-name";
  name.textContent = train.name;
  const number = document.createElement("span");
  number.className = "train-number";
  number.textContent = `Train ${train.number}`;
  identity.append(name, number);

  const route = document.createElement("div");
  route.className = "route-details";
  const times = document.createElement("span");
  times.className = "route-time";
  times.textContent = `${train.departure}  →  ${train.arrival}`;
  const stations = document.createElement("span");
  stations.className = "route-stations";
  stations.textContent = `${train.from}  →  ${train.to}`;
  route.append(times, stations);

  const fare = document.createElement("div");
  const fareLabel = document.createElement("span");
  fareLabel.className = "result-label";
  fareLabel.textContent = "Fare";
  const fareValue = document.createElement("span");
  fareValue.className = "train-fare";
  fareValue.textContent = `₹${train.fare.toLocaleString("en-IN", { minimumFractionDigits: 2 })}`;
  fare.append(fareLabel, fareValue);

  const availability = document.createElement("div");
  const seatLabel = document.createElement("span");
  seatLabel.className = "result-label";
  seatLabel.textContent = "Seats available";
  const seatValue = document.createElement("span");
  seatValue.className = `seat-count${available === 0 ? " sold-out" : ""}`;
  seatValue.textContent = available === 0 ? "Sold out" : String(available);
  availability.append(seatLabel, seatValue);

  card.append(identity, route, fare, availability);
  return card;
}

function showTrains(matches, searching) {
  results.replaceChildren();
  resultsTitle.textContent = searching ? "Search results" : "Popular trains";
  resultsSummary.textContent = searching
    ? `${matches.length} ${matches.length === 1 ? "train" : "trains"} found`
    : "Showing trains from the current timetable";

  if (matches.length === 0) {
    const message = document.createElement("p");
    message.className = "search-message";
    message.textContent = "No trains found for that route. Check the station names and try again.";
    results.append(message);
    return;
  }

  results.append(...matches.map(createResult));
}

form.addEventListener("submit", (event) => {
  event.preventDefault();
  const source = sourceInput.value.trim().toLocaleLowerCase();
  const destination = destinationInput.value.trim().toLocaleLowerCase();
  const matches = trains.filter((train) =>
    train.from.toLocaleLowerCase() === source &&
    train.to.toLocaleLowerCase() === destination
  );
  showTrains(matches, true);
});

showTrains(trains, false);
