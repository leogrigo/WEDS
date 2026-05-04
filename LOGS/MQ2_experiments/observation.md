# Observations

This file contains observation made while testing the consumption of the __mq2__ sensor.

## Avg Current

Current consumption in is average is very stable.
We can assert the sensor requires costantly $122mA$.

### Peeks

The sensor has some peeks in the initial part reaching $124mA$.

$04/05/2026$ It is possible to observe peeks when some spikes happen that can reach up to $128mA$.

## Current Conclusions

After 5min the average current drops from $124mA$ to $122mA$ and stays overall stable with some irrelevant picks or drops very rarely to happen.

## Burn In

The __Burn In__ process is foundamental and mandatory to achieve consistent measures with the mq2.
With further experiments we will assert the optimal _Burn In Time_ in order to maximize the ratio $\frac{accuracy}{current\_consumption}$.

$04/05/2026$ After additional experiments i have observed that, with an interval time of $30min$ the sensor get optimal in $5min$ while the values are acceptable after $3min$.
_I'm considering "Acceptable" readings that are stable in the time and do not visually variates much_.
