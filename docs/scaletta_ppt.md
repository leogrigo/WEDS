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

### Minimum Standard Deviation

Z-Score is not reliable when the baseline is too stable implying a low standard deviation.

To solve this problem we set a minimum standard deviation.

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

We have observed that the wind blows in the same direction for a consistent quantity of time.
This means that on average wind direction remains the same.

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

## The Workflow of the PowerPoint

The ppt should follow this structure:

1. The Idea
2. Sensing
3. Further Implementation

## (1) The Idea

__Goal__: _Detect wildfires in their early stages through the installation of multiple nodes covering different areas._


The idea consist in two main features:

1. Anomaly Detection
2. Risk Computation

### Anomaly Detection

We use anomaly detection to detect the presence of wildfires by the smoke produced by them.

The idea is that smoke is not present, in the most part of the world, unless there is a fire or something that generates it, most likely to be a fire.

Furthermore we can distinguish between a car passing or a fire through the level of gasses in the air.

### Risk Detection

We will use risk detection to predict when a fire is more likely to start.

The idea is that an area where condition are favourable, low humidity and high temperature, a fire is more likely to start. This naive assumption will let us calculate a _'Risk Score'_ that can be used to adapt the sampling frequency. More risk implies higher sampling frequency rates.

To calculate it we will use __TinyML__ to make each node capable of calculating its own risk by itselfes.

## (2) Sensing

### Sensors

We use two sensor:

- __MQ-2__: _Relevates the percentage of gasses present in the environment._
- __AHT20 + BMP280__: _A sensor that is a combination of two sensors, it senses the level of humidity, temperature and pression of the environment._

### How we use them

The two sensors are used for two different tasks.

The __MQ-2__ is used in the __Anomaly Detection__. In fact, as already said, the presence of a wildfire is easily spottable if smoke is detected.

While the other sensor will be used in __Risk Computation__. Humidity and Temperature are two key aspect for a wildfire to start. Combining the two measures and the presence of flammable gasses, with the __MQ-2__, we can calculate a precise __Risk Score__.

### The MQ-2 Problem

The __MQ-2__ need pre-heating in order to work properly. Skipping the warming phase may lead to unstable readings or even totally wrong ones.

We should compute well how many time we measure and when to go to sleep.

### Anomaly Detection Schema

The whole process starts calculating the baseline in a _non-presence of fire_ condition.
With respect to the baseline the standard deviation is calculated. To prevent the Z-Score on being too high due to a low standard deviation, this is set to a predetermined value if below a given treshold.
We are also using an __EMA__ filter to update the baseline favouring the latest read values.
Another problem was that smoke peaks would have modified the baseline drastically. To prevent it, when an anomalous condition is detected, the read values won't be part of the baseline.

After the calculation of the _Z-Score_ the node may set it status to __ANOMALY__ or __NO_ANOMALY__ depending on the _Z-Score_.

Each time the node set is new status, this is communicated to the gateway via LoRa, which keep tracks of the whole network.

### Risk Detection Schema

TBD

### Network Architecture

Nodes -> Gateway (aggregates different perspectives) -> Cloud

## (3) Further Implementation

In program we have two further implementations

### Risk Detect

We already talked about it.

### Inter-Node Communications

The idea is to make possible to near nodes to communicate between them, allowing a node to talk to his _'neighbors'_.

This will enhance coordination between nodes making possible to a node to influence another node.
For example if a node is in a __WARNING__ state, the message can be spread to near nodes in order to make them sample more often.
