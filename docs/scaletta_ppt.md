# Scaletta PPT

0. The refined Idea
1. Architecture
2. Anomaly
3. Communication with LoRa
4. Power Consumption
5. Sensing
6. Further Implementations

## (0)

Two main features:

### Anomaly Detection

_Detecting anomalous scenarios using the MQ-2 sensor to detect smoke spikes._

The idea is that, assuming an optimal sensing position, when a wildfire starts smoke/gas is present in a consistent quantity and the MQ-2 sensor will register a peak that will last for a consistent time.<!--TODO: measure how much time is needed to consider the peak as consistent-->

The MQ-2 is very precise but only when environment condition are stable and do not change.
On the outdoors it is possible to use it but humidity and temperature change may lead to improvise peaks that must be discarded.

To do so the process is: <!-- For n samples >

1. __Computing the Baseline__:

1. Exponential Moving Average
2. Computing Variance and Standard Deviation <!-- the baseline >
3. Computing Z-Score with the Variance and Standard Deviation for each feature

### Risk Computation

_Computing how much in a scenario a wildfire is likely to start._

## (1)

The architecture is star and not manet.

Clustering?

## (6)

Risk Computation feature.

Inter-Node communication: _Two near nodes will be able to communicate to enhance adaptive sampling._