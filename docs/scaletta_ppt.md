# Scaletta PPT

1. Anomaly Detection
2. Communicating with the Gateway
3. Further Implementations

## (1) Anomaly Detection

_Detecting anomalous scenarios using the MQ-2 sensor to detect smoke spikes._

The idea is that, assuming an optimal sensing position, when a wildfire starts smoke/gas is present in a consistent quantity and the MQ-2 sensor will register a peak that will last for a consistent time.<!--TODO: measure how much time is needed to consider the peak as consistent-->

The MQ-2 is very precise but only when environment condition are stable and do not change.
On the outdoors it is possible to use it but humidity and temperature change may lead to improvise peaks that must be discarded.

To do so the process is:

1. __Computing the Baseline__: _The baseline is computed in a scenario considered 'normal'._
2. __Computing the Z-Score__: _Given the baseline, we sample and compute the Z-Score, an high Z-Score will generate a warning._

To not impact the baseline we applied an EMA filter to sensor readings.
An anomalous situation won't characterize the baseline.

The problem now is: _how can we be sure to get the spikes?_
We made some esperiments.

### Experiment 1 (Sensor readings drops exponentially)

We have made an experiment plotting the gas level (buthane) in a slightly ventilated room.

Looking at the two graphs [1 second Graph]("C:\Users\admin\Pictures\Screenshots\Screenshot 2026-04-09 152538.png") and [1 hour Graph]("C:\Users\admin\Pictures\Screenshots\Screenshot 2026-04-09 145056.png") we may see different things:

1. A peak is relevated istantly.
2. Smoke concentration drops slowly.

When the gas is spread near the sensor:

- Peak reach the maximum value.
- After 12 seconds the gas concentration is halved.
- After 105 seconds gas concentration was been nearly dissolved (90%).
- 400 seconds to dissolve it completely

In conclusion we may say that __Smoke Concentration drops Exponentially__.
This implies a larger window to detect a possible spike that happened.

### Experiment 2 (The signal of the fire)

We wanted to observe how a simulated wildfire's fire is detected by the sensor.

## (2) Communicating with the Gateway

Each nodes communicates directly to the gateway via LoRa because the idea is that each node represent an area.

Whenever the node measures the gas levels it may turn into one of two possible states:

- __ANOMALY__: _When the Z-Score is high, a `WARNING` message is sent to the gateway, meaning that the node has detected an anomaly._
- __NO_ANOMALY__: _When the node returns to a non anomalous state._

## (3) What's next?

### Gateway comms

Gateway communications should be discriminated with respect to their importance.

#### Fire and Forget

Every time the anomaly is measured and the state has not changed, the node will send his status as an healtcheck.
The healtcheck is treated as a fire and forget, there is no need to the gateway to know the last state of the node if it hasn't changed.

#### At Least Once

When the status of a node changes, it is communicated to the gateway at least one time.

This ensures that a Warning is correctly delivered to the gateway and further showed to the client.

### Risk Computation Feature

The second main feature.
This will make possible to implement adaptive sampling.

The rule will be simple: _The higher the risk, the higher the sampling rate_.

### Inter-Node Communication

Two near nodes will be able to communicate to enhance adaptive sampling.
