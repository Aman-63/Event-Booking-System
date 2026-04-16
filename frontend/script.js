let events   = [];
let bookings = [];
let activeEvtId = null;

// Stats (loaded from server, persisted)
let statsTotal = 0;
let statsSeat  = 0;
let statsRev   = 0;

// ─── SEAT MAP STATE ──────────────────────────
// Cinema: rows/cols generated dynamically from event.seats
let reservedSeats   = new Set(); // loaded from server
let selectedSeats   = new Set();
let isSelectingMode = false;
let targetSeatCount = 0;

// Concert
const zoneData = {
  'fan-left':  { name:'Fan Zone (Left)',  type:'Standing', price:1500, capacity:150, available:150, color:'var(--green)' },
  'fan-right': { name:'Fan Zone (Right)', type:'Standing', price:1500, capacity:150, available:150, color:'var(--green)' },
  'vip':       { name:'VIP',             type:'Seating',  price:2500, capacity:100, available:100, color:'var(--amber)' },
  'gold':      { name:'Gold Zone',       type:'Standing', price:1000, capacity:300, available:300, color:'var(--blue)'  },
  'silver':    { name:'Silver Zone',     type:'Standing', price:500,  capacity:500, available:500, color:'var(--purple)'},
};
let activeZone  = null;
let ticketCount = 1;

// ─── INIT ─────────────────────────────────────
async function init() {
  await loadStats();
  await loadEvents();
  await loadHistory();
}
init();

// ─── LOAD STATS FROM SERVER ──────────────────
async function loadStats() {
  try {
    let res  = await fetch("/stats");
    let data = await res.json();
    statsTotal = data.bookings || 0;
    statsSeat  = data.seats    || 0;
    statsRev   = data.revenue  || 0;
    updateStats();
  } catch (e) { console.error("Stats fetch error:", e); }
}

// ─── LOAD EVENTS ─────────────────────────────
async function loadEvents() {
  try {
    let res = await fetch("/events");
    if (!res.ok) { console.error("API not working"); return; }
    let data = await res.json();
    if (!Array.isArray(data) || !data.length) {
      document.getElementById("events-grid").innerHTML = "<p style='color:white'>No events available</p>";
      return;
    }
    events = data.map(e => ({
      id: e.id, name: e.name||"Unknown", date: e.date||"N/A",
      venue: e.venue||"N/A", price: e.price||0, seats: e.seats||0,
      type: guessType(e.name)
    }));
    renderEvents();
  } catch (err) { console.error("Fetch error:", err); }
}

function guessType(name) {
  return (name||"").toLowerCase().includes("concert") ? "concert" : "movie";
}

// ─── RENDER EVENTS ───────────────────────────
function renderEvents() {
  const grid = document.getElementById('events-grid');
  if (!grid) return;
  if (!events.length) { grid.innerHTML = "<p style='color:white'>No events found</p>"; return; }
  grid.innerHTML = '';
  events.forEach(ev => {
    grid.innerHTML += `
    <div class="event-card ${ev.type}">
      <div class="etype-badge ${ev.type}">${ev.type==='concert'?'♪ Concert':'▶ '+ev.name}</div>
      <div class="event-name">${ev.name}</div>
      <div class="event-meta">${ev.date} · ${ev.venue}</div>
      <div class="details-grid">
        <div class="detail-box"><div class="detail-lbl">Price</div><div class="detail-val">₹${ev.price}</div></div>
        <div class="detail-box"><div class="detail-lbl">Seats left</div><div class="detail-val">${ev.seats}</div></div>
      </div>
      <div class="card-btns">
        <button class="btn btn-primary ${ev.type}" onclick="openBook(${ev.id})">Book tickets</button>
        <button class="btn btn-secondary" onclick="openCancel(${ev.id})">Cancel</button>
      </div>
    </div>`;
  });
}

// ─── LOAD HISTORY ────────────────────────────
async function loadHistory() {
  try {
    let res  = await fetch("/history");
    let data = await res.json();
    bookings = data.map(b => ({ ...b, time: b.time || "" }));
    renderHistory();
    updateHistoryBadge();
  } catch (err) { console.error("History fetch error:", err); }
}

// ─── OPEN BOOK MODAL ─────────────────────────
async function openBook(id) {
  activeEvtId = id;
  const ev = events.find(e => e.id == id);
  if (!ev) return;

  // Header
  const badge = document.getElementById('m-badge');
  badge.className   = `etype-badge ${ev.type}`;
  badge.textContent = ev.type==='concert' ? '♪ Concert' : '▶ '+ev.name;
  document.getElementById('m-name').textContent = ev.name;
  document.getElementById('m-meta').textContent = `${ev.date} · ${ev.venue}`;
  document.getElementById('inp-name').value = '';
  document.getElementById('m-confirm-btn').className = `modal-action ${ev.type}`;

  if (ev.type === 'concert') {
    document.getElementById('cinema-map-section').style.display  = 'none';
    document.getElementById('concert-map-section').style.display = 'block';
    resetConcertMap();
  } else {
    document.getElementById('cinema-map-section').style.display  = 'block';
    document.getElementById('concert-map-section').style.display = 'none';
    resetCinemaMap();
    // Fetch already-booked seats for this event from server
    try {
      let res  = await fetch(`/seatmap?eventId=${id}`);
      let data = await res.json(); // array of seat label strings e.g. ["A1","A2"]
      reservedSeats = new Set(data);
    } catch(e) {
      reservedSeats = new Set();
    }
    // Build grid dynamically from event total seats
    buildSeatGrid(ev);
  }

  document.getElementById('book-overlay').classList.add('open');
}

// ─── CONFIRM BOOK ────────────────────────────
async function confirmBook() {
  const name = document.getElementById('inp-name').value.trim();
  if (!name) { showToast("Enter your name", "error"); return; }

  const ev = events.find(e => e.id == activeEvtId);
  if (!ev) return;

  let seatsToBook  = 0;
  let seatLabels   = "";

  if (ev.type === 'concert') {
    if (!activeZone) { showToast("Select a zone first", "error"); return; }
    seatsToBook = ticketCount;
    seatLabels  = "";
  } else {
    if (selectedSeats.size === 0) { showToast("Select at least one seat", "error"); return; }
    if (selectedSeats.size < targetSeatCount) {
      showToast(`Select all ${targetSeatCount} seat(s) first`, "error"); return;
    }
    seatsToBook = selectedSeats.size;
    seatLabels  = [...selectedSeats].sort().join(",");
  }

  let res = await fetch("/book", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user: name, eventId: activeEvtId, seats: seatsToBook, seatLabels })
  });
  let msg = await res.text();

  if (msg === "BOOKED") {
    const time = new Date().toLocaleTimeString();
    bookings.unshift({ user: name, eventName: ev.name, eventId: activeEvtId, seats: seatsToBook, time });
    // Refresh stats from server (server already updated stats.txt)
    await loadStats();
    renderHistory();
    updateHistoryBadge();
    closeModals();
    loadEvents();
    showToast("Booking confirmed!", "success");
  } else if (msg === "FAILED") {
    showToast("Not enough seats available", "error");
  } else {
    showToast("Booking failed: " + msg, "error");
  }
}

// ─── BUILD CINEMA SEAT GRID ──────────────────
// Dynamically generate rows/cols from ev.seats total
function buildSeatGrid(ev) {
  const grid = document.getElementById('seat-grid');
  if (!grid) return;
  grid.innerHTML = '';

  // Total seats available in the event originally
  // We use a fixed 12-col layout and compute rows needed
  const COLS = 12;
  // Compute total capacity = ev.seats + already booked
  const bookedCount  = reservedSeats.size;
  const totalSeats   = ev.seats + bookedCount; // original total
  const rows         = Math.ceil(totalSeats / COLS);
  const ROWNAMES     = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';

  // Column labels
  const colLabelRow = document.createElement('div');
  colLabelRow.className = 'col-labels';
  colLabelRow.innerHTML = '<div></div><div></div>';
  for (let c = 1; c <= Math.min(5, COLS); c++)
    colLabelRow.innerHTML += `<div class="col-lbl">${c}</div>`;
  colLabelRow.innerHTML += '<div></div>';
  for (let c = 6; c <= COLS; c++)
    colLabelRow.innerHTML += `<div class="col-lbl">${c}</div>`;
  grid.appendChild(colLabelRow);

  let seatNum = 0;
  for (let r = 0; r < rows; r++) {
    if (r > 0 && r % 5 === 0) {
      const gap = document.createElement('div');
      gap.className = 'section-gap';
      grid.appendChild(gap);
    }

    const rowEl = document.createElement('div');
    rowEl.className = 'seat-row';

    const lbl = document.createElement('div');
    lbl.className = 'row-lbl';
    lbl.textContent = ROWNAMES[r] || String(r+1);
    rowEl.appendChild(lbl);

    // Aisle before col 1
    rowEl.appendChild(Object.assign(document.createElement('div'), {className:'aisle'}));

    for (let c = 1; c <= COLS; c++) {
      if (seatNum >= totalSeats) {
        // Placeholder to keep grid aligned
        const ph = document.createElement('div');
        ph.style.width = '28px';
        rowEl.appendChild(ph);
      } else {
        const seatId = `${ROWNAMES[r]}${c}`;
        rowEl.appendChild(makeSeat(seatId));
      }
      seatNum++;
      if (c === 5) rowEl.appendChild(Object.assign(document.createElement('div'), {className:'aisle'}));
    }
    grid.appendChild(rowEl);
  }

  // Update legend stats
  const statsEl = document.getElementById('legend-stats');
  if (statsEl) statsEl.innerHTML = `Rows: A–${ROWNAMES[rows-1]||rows}<br>Cols: 1–${COLS}<br>Total: ${totalSeats}<br>Booked: ${bookedCount}`;
}

function makeSeat(seatId) {
  const el = document.createElement('div');
  el.className = 'seat';
  el.dataset.id = seatId;
  if (reservedSeats.has(seatId)) el.classList.add('reserved');
  el.addEventListener('click', () => toggleSeat(seatId, el));
  return el;
}

function toggleSeat(seatId, el) {
  if (!isSelectingMode) {
    showToast('Enter seat count then click "Start Selecting"', 'error'); return;
  }
  if (reservedSeats.has(seatId)) return;
  if (selectedSeats.has(seatId)) {
    selectedSeats.delete(seatId);
    el.classList.remove('selected');
  } else {
    if (selectedSeats.size >= targetSeatCount) {
      showToast(`You can only select ${targetSeatCount} seat(s)`, 'error'); return;
    }
    selectedSeats.add(seatId);
    el.classList.add('selected');
  }
  updateSeatInfo();
}

function updateSeatInfo() {
  const seats = [...selectedSeats].sort();
  const tag = document.getElementById('selected-tag');
  if (tag) tag.textContent = seats.length ? seats.join(', ') : '—';
  const hint = document.getElementById('cinema-hint-modal');
  if (hint) hint.textContent = selectedSeats.size < targetSeatCount
    ? `Select ${targetSeatCount - selectedSeats.size} more seat(s)`
    : `All ${targetSeatCount} seat(s) selected — click Confirm!`;
  const cr = document.getElementById('confirm-row');
  if (cr) cr.style.display = 'flex';
}

function startSelecting() {
  const cnt = parseInt(document.getElementById('c-seat-count-modal').value);
  if (!cnt || cnt < 1 || cnt > 20) { showToast('Enter a valid seat count (1–20)', 'error'); return; }
  selectedSeats.clear();
  document.querySelectorAll('#seat-grid .seat.selected').forEach(s => s.classList.remove('selected'));
  document.querySelectorAll('#seat-grid .seat').forEach(s => s.classList.add('selecting-mode'));
  isSelectingMode = true;
  targetSeatCount = cnt;
  const hint    = document.getElementById('cinema-hint-modal');
  const hintBar = document.getElementById('cinema-hint-bar');
  const cr      = document.getElementById('confirm-row');
  const tag     = document.getElementById('selected-tag');
  if (hint)    hint.textContent    = `Select ${cnt} seat(s) on the map`;
  if (hintBar) hintBar.style.display = 'block';
  if (cr)      cr.style.display    = 'flex';
  if (tag)     tag.textContent     = '—';
  showToast(`Selecting mode — pick ${cnt} seat(s)`, 'success');
}

function resetCinemaMap() {
  selectedSeats.clear();
  isSelectingMode = false;
  targetSeatCount = 0;
  reservedSeats   = new Set();
  const hintBar = document.getElementById('cinema-hint-bar');
  const cr      = document.getElementById('confirm-row');
  const input   = document.getElementById('c-seat-count-modal');
  if (hintBar) hintBar.style.display = 'none';
  if (cr)      cr.style.display      = 'none';
  if (input)   input.value           = 1;
  const grid = document.getElementById('seat-grid');
  if (grid) grid.innerHTML = '';
}

// ─── CONCERT MAP LOGIC ───────────────────────
function selectZone(zid) {
  activeZone  = zid;
  ticketCount = 1;
  document.querySelectorAll('.zone-block').forEach(z => z.classList.remove('selected-zone'));
  const zoneEl = document.getElementById(`zone-${zid}`);
  if (zoneEl) zoneEl.classList.add('selected-zone');
  const z = zoneData[zid];
  document.getElementById('zone-placeholder').style.display = 'none';
  document.getElementById('zone-details').style.display     = 'block';
  document.getElementById('zi-name').textContent  = z.name;
  document.getElementById('zi-name').style.color  = z.color;
  document.getElementById('zi-type').textContent  = z.type;
  document.getElementById('zi-price').textContent = `₹${z.price.toLocaleString()}`;
  document.getElementById('zi-cap').textContent   = z.capacity;
  document.getElementById('zi-avail').textContent = z.available;
  const pct = (z.available / z.capacity) * 100;
  document.getElementById('zi-bar').style.width      = pct + '%';
  document.getElementById('zi-bar').style.background = z.color;
  document.getElementById('ticket-num').textContent  = 1;
  updatePricePreview();
}

function adjTickets(delta) {
  if (!activeZone) return;
  const z = zoneData[activeZone];
  ticketCount = Math.max(1, Math.min(ticketCount + delta, z.available, 20));
  document.getElementById('ticket-num').textContent = ticketCount;
  updatePricePreview();
}

function updatePricePreview() {
  if (!activeZone) return;
  const z = zoneData[activeZone];
  document.getElementById('pr-unit').textContent  = `₹${z.price.toLocaleString()}`;
  document.getElementById('pr-count').textContent = ticketCount;
  document.getElementById('pr-total').textContent = `₹${(z.price * ticketCount).toLocaleString()}`;
}

function resetConcertMap() {
  activeZone  = null;
  ticketCount = 1;
  document.querySelectorAll('.zone-block').forEach(z => z.classList.remove('selected-zone'));
  const zp = document.getElementById('zone-placeholder');
  const zd = document.getElementById('zone-details');
  if (zp) zp.style.display = 'block';
  if (zd) zd.style.display = 'none';
}

// ─── CANCEL ──────────────────────────────────
let cSeatCountVal = 1;
async function confirmCancel() {
  const name = document.getElementById('cinp-name').value.trim();
  if (!name) { showToast("Enter your name", "error"); return; }
  const ev = events.find(e => e.id == activeEvtId);

  let res = await fetch("/cancel", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user: name, eventId: activeEvtId, seats: cSeatCountVal })
  });
  let msg = await res.text();

  if (msg === "CANCELLED") {
    await loadStats();
    closeModals();
    loadEvents();
    loadHistory();
    showToast("Cancelled successfully", "success");
  } else if (msg === "NOT_FOUND") {
    showToast("No matching booking found", "error");
  } else {
    showToast("Cancel failed: " + msg, "error");
  }
}

// ─── ADD EVENT ───────────────────────────────
async function submitEvent() {
  const name  = document.getElementById("e-name").value.trim();
  const date  = document.getElementById("e-date").value.trim();
  const venue = document.getElementById("e-venue").value.trim();
  const price = document.getElementById("e-price").value;
  const seats = document.getElementById("e-seats").value;
  if (!name||!date||!venue||!price||!seats) { showToast("Fill all fields","error"); return; }
  let res = await fetch("/addEvent", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({name,date,venue,price,seats})
  });
  let msg = await res.text();
  if (msg==="EVENT_ADDED") { showToast("Event added!","success"); closeAddEvent(); loadEvents(); }
  else showToast("Error: "+msg,"error");
}

// ─── CLEAR HISTORY (UI only) ─────────────────
function clearHistory() {
  bookings = [];
  renderHistory();
  updateHistoryBadge();
  showToast("Display cleared (server history unchanged)","success");
}

// ─── UI HELPERS ──────────────────────────────
function adjCancelSeats(change) {
  cSeatCountVal = Math.max(1, cSeatCountVal + change);
  document.getElementById('cseat-disp').textContent = cSeatCountVal;
}

function updateHistoryBadge() {
  const cnt = document.getElementById('history-cnt');
  cnt.textContent   = bookings.length;
  cnt.style.display = bookings.length ? 'inline' : 'none';
}

function openCancel(id) {
  activeEvtId   = id;
  cSeatCountVal = 1;
  document.getElementById('cseat-disp').textContent = 1;
  document.getElementById('cinp-name').value = '';
  const ev = events.find(e => e.id == id);
  if (ev) {
    const badge = document.getElementById('cm-badge');
    badge.className   = `etype-badge ${ev.type}`;
    badge.textContent = ev.type==='concert' ? '♪ Concert' : '▶ '+ev.name;
    document.getElementById('cm-name').textContent = ev.name;
    document.getElementById('cm-meta').textContent = `${ev.date} · ${ev.venue}`;
  }
  document.getElementById('cancel-overlay').classList.add('open');
}

function closeModals() {
  document.getElementById('book-overlay').classList.remove('open');
  document.getElementById('cancel-overlay').classList.remove('open');
}

function openAddEvent()  { document.getElementById("add-overlay").classList.add("open"); }
function closeAddEvent() { document.getElementById("add-overlay").classList.remove("open"); }

function switchTab(tab) {
  document.getElementById('events-wrap').style.display  = tab==='events'  ? 'block' : 'none';
  document.getElementById('history-wrap').style.display = tab==='history' ? 'block' : 'none';
  document.querySelectorAll('.nav-btn').forEach((btn,i) => {
    btn.classList.toggle('active', (tab==='events'&&i===0)||(tab==='history'&&i===1));
  });
}

function updateStats() {
  document.getElementById('stat-bookings').textContent = statsTotal;
  document.getElementById('stat-seats').textContent    = statsSeat;
  document.getElementById('stat-revenue').textContent  = statsRev;
}

function renderHistory() {
  const list = document.getElementById('history-list');
  if (!bookings.length) { list.innerHTML="<p style='color:gray;padding:1rem;'>No booking history yet</p>"; return; }
  list.innerHTML = bookings.map(b => `
    <div class="history-item">
      <div class="h-avatar ${guessType(b.eventName)}">${(b.user||'?')[0].toUpperCase()}</div>
      <div class="h-info">
        <div class="h-user">${b.user}</div>
        <div class="h-event">${b.eventName||'Event #'+b.eventId}</div>
      </div>
      <div class="h-right">
        <div class="h-seats">${b.seats} seat${b.seats>1?'s':''}</div>
        <div class="h-time">${b.time||''}</div>
      </div>
    </div>`).join('');
}

function showToast(msg, type) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className   = `toast ${type} show`;
  setTimeout(() => t.classList.remove('show'), 2500);
}