// MQTT Configuration
const MQTT_BROKER = "broker.benax.rw";
const MQTT_PORT = 9001; // WebSocket port on broker.benax.rw
const MQTT_PATH = "/mqtt";
const MQTT_TOPIC = "uwase/sonia/temperature";
const CLIENT_ID = "web_dash_" + Math.random().toString(16).substr(2, 8);

// DOM Elements
const statusText = document.getElementById("status-text");
const connectionIndicator = document.getElementById("connection-indicator");
const tempVal = document.getElementById("temp-val");
const lastUpdated = document.getElementById("last-updated");

// Initialize Chart.js
const ctx = document.getElementById('tempChart').getContext('2d');
const maxDataPoints = 15;
const chartData = {
    labels: [],
    datasets: [{
        label: 'Temperature (°C)',
        data: [],
        borderColor: '#3b82f6',
        backgroundColor: 'rgba(59, 130, 246, 0.15)',
        borderWidth: 3,
        fill: true,
        tension: 0.4,
        pointBackgroundColor: '#ffffff',
        pointBorderColor: '#3b82f6',
        pointHoverBackgroundColor: '#3b82f6',
        pointHoverBorderColor: '#ffffff',
        pointRadius: 4,
        pointHoverRadius: 6
    }]
};

const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
        legend: {
            display: false
        }
    },
    scales: {
        x: {
            grid: {
                color: 'rgba(255, 255, 255, 0.04)'
            },
            ticks: {
                color: '#9ca3af'
            }
        },
        y: {
            grid: {
                color: 'rgba(255, 255, 255, 0.04)'
            },
            ticks: {
                color: '#9ca3af'
            },
            suggestedMin: 15,
            suggestedMax: 35
        }
    }
};

const tempChart = new Chart(ctx, {
    type: 'line',
    data: chartData,
    options: chartOptions
});

// Initialize Paho MQTT Client
const client = new Paho.MQTT.Client(MQTT_BROKER, MQTT_PORT, MQTT_PATH, CLIENT_ID);

// Set Callback Handlers
client.onConnectionLost = onConnectionLost;
client.onMessageArrived = onMessageArrived;

// Connect the Client
connectMQTT();

function connectMQTT() {
    statusText.innerText = "Connecting...";
    statusText.className = "status-disconnected";
    connectionIndicator.classList.remove("connected");

    const options = {
        useSSL: false, // broker.benax.rw uses plain WebSockets (ws://)
        onSuccess: onConnectSuccess,
        onFailure: onConnectFailure,
        keepAliveInterval: 30
    };

    try {
        client.connect(options);
    } catch (e) {
        console.error("MQTT Connection Setup Error:", e);
        statusText.innerText = "Setup Error";
    }
}

// Connection Success Callback
function onConnectSuccess() {
    console.log("[MQTT] Connected to broker.");
    statusText.innerText = "Connected";
    statusText.className = "status-connected";
    connectionIndicator.classList.add("connected");

    // Subscribe to the Temperature Topic
    client.subscribe(MQTT_TOPIC);
    console.log(`[MQTT] Subscribed to topic: ${MQTT_TOPIC}`);
}

// Connection Failure Callback
function onConnectFailure(error) {
    console.error("[MQTT] Connection failed:", error);
    statusText.innerText = "Failed";
    statusText.className = "status-disconnected";
    
    // Retry connection after 5 seconds
    setTimeout(connectMQTT, 5000);
}

// Connection Lost Callback
function onConnectionLost(responseObject) {
    console.warn("[MQTT] Connection lost:", responseObject.errorMessage);
    statusText.innerText = "Disconnected";
    statusText.className = "status-disconnected";
    connectionIndicator.classList.remove("connected");
}

// Message Arrived Callback
function onMessageArrived(message) {
    const payloadStr = message.payloadString;
    console.log(`[MQTT] Received message: ${payloadStr}`);
    
    let temperature = null;
    let timestamp = new Date().toLocaleTimeString();

    // Attempt to parse JSON payload
    try {
        const data = JSON.parse(payloadStr);
        if (data && typeof data.temperature !== 'undefined') {
            temperature = parseFloat(data.temperature);
        }
        if (data && data.timestamp) {
            // Check if timestamp is epoch
            if (typeof data.timestamp === 'number') {
                timestamp = new Date(data.timestamp * 1000).toLocaleTimeString();
            } else {
                timestamp = data.timestamp;
            }
        }
    } catch (e) {
        // Fallback: If not JSON, parse direct float value
        const parsed = parseFloat(payloadStr);
        if (!isNaN(parsed)) {
            temperature = parsed;
        }
    }

    if (temperature !== null && !isNaN(temperature)) {
        // Update LCD UI Elements
        tempVal.innerText = temperature.toFixed(1);
        lastUpdated.innerText = timestamp;

        // Visual flash animation on update
        tempVal.style.color = '#60a5fa';
        setTimeout(() => {
            tempVal.style.color = '#ffffff';
        }, 300);

        // Update Chart Data
        chartData.labels.push(timestamp);
        chartData.datasets[0].data.push(temperature);

        // Maintain fixed number of maximum historical points
        if (chartData.labels.length > maxDataPoints) {
            chartData.labels.shift();
            chartData.datasets[0].data.shift();
        }

        // Refresh Chart rendering
        tempChart.update();
    }
}
