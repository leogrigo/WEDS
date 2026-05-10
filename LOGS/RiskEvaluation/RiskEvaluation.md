# Risk Evaluation

__Risk Evaluation__ is the second main feature of WEDS.

## Overview

The idea is to compute a _Risk Score_ that will characterize nodes' sampling frequency.

### Assumptions

The idea will work thanks to two assumptions:

1. An higher _Risk Score_ implies that a wildfire is more likely to happen.
2. Where a wildfire is more likely to start the node should sample more frequently.

This two naive assumptions are enough to define the goal and how to accomplish it.

### Goal

The main goal of _Risk Evaluation_ task can be defined as:

    "The computed Risk should be directly proportional to the probability that a Wildfire will start in order to minimize the occurence of false negatives with respect to energy consumption by optimizing the sampling frequency."

## Process

The task will follow some simple main steps, assuming the model is already installed:

1. Measure environmental data.
2. Normalize and clean data.
3. Compute the _Risk Score_ using the model.
4. Adapt _Sampling Frequency_ with respect to the _Risk Score_.

### Environmental Clean Data

Data will be measured and then normalized and cleansed.
We expect stable data due to the fact that humidity and temperature changes very slowly over the day.

### Compute

The score will be directly computed by the model.

### Adapt

This will be the thoughest part, there are two main problems:

1. Should there be a threshold whenever the risk is too low?
2. How is _Risk Score_ connected to the probability of starting wildfires?

__(1)__ There is always a probability that a wildfire is not started only by natural causes but by human intervation even if conditions are not favourable. How much should the system be able to detect this scenario? placing a threshold will imply more consumption when not needed just to detect a very rare event.

__(2)__ What is the connection between the two parameters? Should it be the ratio between number of wildfire started and not within certain conditions? Or something else?

## Dataset

//TODO

## LOGS

### V1

    Confusion Matrix:
                precision    recall  f1-score   support

            0       1.00      0.58      0.73   1296406
            1       0.00      0.72      0.01      2708

        accuracy                           0.58   1299114
    macro avg       0.50      0.65      0.37   1299114
    weighted avg       1.00      0.58      0.73   1299114

![matrix](assets/graphs/matrix1.png)

### V2

    Confusion Matrix:
                precision    recall  f1-score   support

            0       1.00      0.61      0.75   1296406
            1       0.00      0.69      0.01      2708

        accuracy                           0.61   1299114
    macro avg       0.50      0.65      0.38   1299114
    weighted avg       1.00      0.61      0.75   1299114

![matrix2](assets/graphs/matrix2.png)