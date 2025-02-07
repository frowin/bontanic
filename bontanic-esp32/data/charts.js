// Initialize data arrays for each sensor
const maxDataPoints = 20;
const tempData = Array(maxDataPoints).fill(null);
const humidData = Array(maxDataPoints).fill(null);
const soilData = Array(maxDataPoints).fill(null);
const labels = Array(maxDataPoints).fill('');

// Chart configuration
const chartConfig = {
    type: 'line',
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
            legend: {
                display: false
            }
        },
        scales: {
            y: {
                beginAtZero: false,
                grid: {
                    color: 'rgba(46,125,50,0.1)'
                }
            },
            x: {
                display: false
            }
        }
    }
};

// Create charts
const tempChart = new Chart(document.getElementById('tempChart'), {
    ...chartConfig,
    data: {
        labels: labels,
        datasets: [{
            data: tempData,
            borderColor: '#2E7D32',
            borderWidth: 2,
            fill: false,
            tension: 0.4
        }]
    }
});

const humidChart = new Chart(document.getElementById('humidChart'), {
    ...chartConfig,
    data: {
        labels: labels,
        datasets: [{
            data: humidData,
            borderColor: '#2E7D32',
            borderWidth: 2,
            fill: false,
            tension: 0.4
        }]
    }
});

const soilChart = new Chart(document.getElementById('soilChart'), {
    ...chartConfig,
    data: {
        labels: labels,
        datasets: [{
            data: soilData,
            borderColor: '#2E7D32',
            borderWidth: 2,
            fill: false,
            tension: 0.4
        }]
    }
});

// Update the message handling
window.addEventListener('message', function(event) {
    try {
        let myObj = event.data;
        // Check if the data is a string and needs parsing
        if (typeof event.data === 'string') {
            myObj = JSON.parse(event.data);
        }
        
        // Update chart data
        if (myObj.temperature) {
            tempData.push(parseFloat(myObj.temperature));
            tempData.shift();
            tempChart.update();
            console.log('Temperature data:', tempData);
        }
        
        if (myObj.humidity) {
            humidData.push(parseFloat(myObj.humidity));
            humidData.shift();
            humidChart.update();
            console.log('Humidity data:', humidData);
        }
        
        if (myObj.soil) {
            soilData.push(parseFloat(myObj.soil));
            soilData.shift();
            soilChart.update();
            console.log('Soil data:', soilData);
        }
    } catch (error) {
        console.error('Error processing message:', error);
    }
});