[← Back to Components](../README.md)

# Sensirion SEN6x ESPHome Component

Integration for Sensirion SEN6x air quality sensors (I²C only; SEN6x does not support UART).

## Supported variants
- SEN62
- SEN63C
- SEN65
- SEN66
- SEN68
- SEN69C

## Configuration variables
All sensor options are optional and follow ESPHome Sensor schema unless noted.

### Sensors
- `pm_1_0`, `pm_2_5`, `pm_4_0`, `pm_10_0`
- `temperature`, `humidity`
- `voc` (optional `algorithm_tuning`)
- `nox` (optional `algorithm_tuning`)
- `co2`
- `hcho`
- `measurement_running` (binary sensor)

### Algorithm tuning
Under `voc:` and `nox:`
- `algorithm_tuning:`
	- `index_offset`
	- `learning_time_offset_hours`
	- `learning_time_gain_hours`
	- `gating_max_duration_minutes`
	- `std_initial`
	- `gain_factor`

### Temperature compensation
```
temperature_compensation:
	offset: 0
	normalized_offset_slope: 0
	time_constant: 0
	slot: 0
```
Valid ranges:
- `offset`: −163.84 … 163.835 °C
- `normalized_offset_slope`: −3.2768 … 3.2767
- `time_constant`: 0 … 65535 s
- `slot`: 0 … 4

### Temperature acceleration
```
temperature_acceleration:
	k: 0
	p: 0
	t1: 0
	t2: 0
```
Valid ranges: 0 … 6553.5 (each is scaled by 10 internally).

### CO₂ compensation settings
- `ambient_pressure`: 700 … 1200 (hPa)
- `sensor_altitude`: 0 … 3000 (m)
- `co2_automatic_self_calibration`: true/false

### Other
- `store_baseline`: true/false (default true)
- `update_interval`: standard ESPHome polling interval
- `startup_delay`: time to wait after each start before publishing values (default 60s)
- `address`: I²C address (default 0x6B)
- `auto_cleaning`:
	- `enabled`: true/false (default true)
	- `interval`: default 1 week
	- Note: implemented in ESPHome (SEN6x has no auto-cleaning interval command)

## Actions
- `sen6x.start_fan_autoclean`
- `sen6x.perform_forced_co2_recalibration`
- `sen6x.co2_sensor_factory_reset`
- `sen6x.activate_sht_heater`
- `sen6x.get_sht_heater_measurements`
- `sen6x.start_measurement`
- `sen6x.stop_measurement`

Note: Actions that require idle mode will stop measurement internally. Call `sen6x.start_measurement` again after running them.

### Action examples
```
on_boot:
	then:
		- sen6x.perform_forced_co2_recalibration:
				id: sen6x_1
				reference_co2: 400
		- sen6x.co2_sensor_factory_reset:
				id: sen6x_1
		- sen6x.activate_sht_heater:
				id: sen6x_1
		- sen6x.get_sht_heater_measurements:
				id: sen6x_1
		- sen6x.start_measurement:
				id: sen6x_1
		- sen6x.stop_measurement:
				id: sen6x_1
```

### Energy-saving example (start every 5 min, run 2 min)
```
interval:
  - interval: 5min
    then:
      - sen6x.start_measurement:
          id: sen6x_1
      - delay: 2min
      - sen6x.stop_measurement:
          id: sen6x_1
```

## Example configuration
```
sensor:
	- platform: sen6x
		id: sen6x_1
		pm_1_0:
			name: PM1.0
		pm_2_5:
			name: PM2.5
		pm_4_0:
			name: PM4.0
		pm_10_0:
			name: PM10.0
		temperature:
			name: Temperature
		humidity:
			name: Humidity
		voc:
			name: VOC
			algorithm_tuning:
				index_offset: 100
				learning_time_offset_hours: 12
				learning_time_gain_hours: 12
				gating_max_duration_minutes: 180
				std_initial: 50
				gain_factor: 230
		nox:
			name: NOx
		co2:
			name: CO2
		hcho:
			name: HCHO
		temperature_compensation:
			offset: 0
			normalized_offset_slope: 0
			time_constant: 0
			slot: 0
		temperature_acceleration:
			k: 0
			p: 0
			t1: 0
			t2: 0
		ambient_pressure: 1013
		sensor_altitude: 0
		co2_automatic_self_calibration: true
		startup_delay: 1min
		auto_cleaning:
			enabled: true
			interval: 1week
		measurement_running:
			name: "SEN6x Measurement Running"
		store_baseline: true
		update_interval: 15s
```
