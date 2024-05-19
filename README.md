# BeagleSky: Next-Gen Weather Monitoring

## Project Overview
This project develops a compact, BeagleBone Black-powered weather station that monitors and logs atmospheric conditions such as temperature, pressure, and light intensity. The data is displayed on an LCD and stored in CSV format for further analysis.

## Hardware Components
- **BeagleBone Black**: Central processing unit.
- **Sensors**:
  - **BMP280**: Measures temperature and barometric pressure.
  - **LDR (Light Dependent Resistor)**: Monitors light intensity.
- **LCD Display (1602)**: Displays the sensor readings.
- **Wires and Resistors**: For connecting components.

## Setup and Execution
### Hardware Setup
- **LCD Connection**: GPIO pins are used for interfacing with the LCD.
- **LDR Connection**: Connected to ANIO with a 10k ohm resistor to GND.
- **BMP280 Connection**: Connected to VCC, GND, SDA, and SCL.

### Compilation and Execution

#### Compile the sensor code with:
    gcc -o bmp_example bmp_example.c bmp280.c bmp_commons.c -lpthread

#### Run the compiled program:
    sudo ./bmp_example

## Testing and Deployment
- **Data Collection**: Sensors collect data at predefined intervals.
- **Display Management**: LCD updates in real-time to show the latest sensor readings.
- **Testing**: Includes both nominal and off-nominal conditions to ensure sensor accuracy and system robustness.

## Conclusion
The project successfully logs key environmental data using BeagleBone Black, showcasing real-time and historical data analysis.

## Future Work
Plans include expanding capabilities to incorporate cloud-based data services, additional environmental parameters, and a web interface for remote data monitoring.

## Demonstration
[Weather Station Demonstration Video!](https://drive.google.com/file/d/1Ji34xWXW-_yL10AchFpvq3xL-OhcE11y/view)



