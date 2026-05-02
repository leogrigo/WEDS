# Observations

This file contains observation made while testing the consumption of the __mq2__ sensor.

## Avg Current

Current consumption in is average is very stable.
We can assert the sensor requires costantly $122mA$.

### Peeks

The sensor has some peeks in the initial part reaching $124mA$.

## Current Conclusions

After 5min the average current drops from $124mA$ to $122mA$ and stays overall stable with some irrelevant picks or drops very rarely to happen.

## Burn In

The __Burn In__ process is foundamental and mandatory to achieve consistent measures with the mq2.
With further experiments we will assert the optimal _Burn In Time_ in order to maximize the ratio $\frac{accuracy}{current\_consumption}$.
