// ChirpStack Application Server Decoder Function
// For LoRaWAN Picobox Temperature & Humidity Sensor

function Decode(fPort, bytes, variables) {
    // Check if we have the expected 4 bytes
    if (bytes.length !== 4) {
        return {
            error: "Invalid payload length. Expected 4 bytes, got " + bytes.length
        };
    }
    
    // Decode humidity from bytes 0-1 (big-endian, divide by 100)
    var humidityRaw = (bytes[0] << 8) | bytes[1];
    var humidity = humidityRaw / 100.0;
    
    // Decode temperature from bytes 2-3 (big-endian, divide by 100)
    var temperatureRaw = (bytes[2] << 8) | bytes[3];
    var temperature = temperatureRaw / 100.0;
    
    // Return decoded values
    return {
        humidity: humidity,
        temperature: temperature,
        unit_humidity: "%",
        unit_temperature: "°C"
    };
}

// Alternative function name for different ChirpStack versions
function decodeUplink(input) {
    return {
        data: Decode(input.fPort, input.bytes, input.variables)
    };
}

// Test function (remove this in production)
function test() {
    // Test with example payload: humidity=73.3%, temperature=23.3°C
    // This would be sent as: [0x1C, 0xA5, 0x09, 0x1D] in hex
    var testBytes = [0x1C, 0xA5, 0x09, 0x1D];
    var result = Decode(1, testBytes, {});
    console.log("Test result:", result);
    // Expected: {humidity: 73.3, temperature: 23.3, unit_humidity: "%", unit_temperature: "°C"}
}

// For ChirpStack v3 compatibility
function Decoder(bytes, port) {
    return Decode(port, bytes, {});
}
