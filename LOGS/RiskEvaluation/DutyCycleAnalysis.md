---
tags:
    - WEDS
---

# Duty Cycle Analysis

As we have seen from the [risk computation analysis](/LOGS/RiskEvaluation/RiskScoreComputation.md) we can distinguish three different tresholds of risk:

| Low Risk | Medium Risk | High Risk |
| :---: | :---: | :---: |
| $\text{risk\_score} < 0.35$ | $0.35 < \text{risk\_score} < 0.83$ | $\text{risk\_score} > 0.83$ |
| $\text{risk}< 0.12\%$ | $\text{risk}< 0.79\%$ | $\text{risk}\le 3.08\%$ |

## Baseline Risk

We may define the baseline risk as $\text{baseline\_risk} = 0.21%$, where the baseline risk is the quantity of fires over total records in the whole dataset.

To make calculations simple, we may assume that the _baseline risk_ represents the ground truth.

Furthermore we can observe that dataset records are hourly taken, so we can state that If

## The Goal

The main goal of WEDS is to _"Detect wildfires as long as they are in a manageable state"_. This can be reduced to satisfy two requirements:

1. _Flame length_ under $3$ Meters.
2. _Burned Area_ is below $100$ Acres.

Statistically $98\%$ of wildfires are quickly suppressed if they do not exceed $100$ acres [^fwd] and if their flames do not exceed $3$ meters human intervention is possible withouth the help of __Water Dropper__ [^novascotia].

## How to achieve the goal with the model

Fire spreading can be measured by the same feature on which the model has been trained, __temperature__ and __humidity__, so the ___risk score___ computed by the model corresponds to the actual risk of wildfire spreading [^iere].

An __high risk score__ will match an __high risk scenario__.

### The wind problem

We are aware that __wind__ is a crucial feature to predict fire spreading risk and we have considered the option to measure it in some way but without success.

### High Risk Scenario

We now need to map the __high risk__ computed to a practical wildfire spread speed.

__Case Study:__ Camp Fire, California, 08/11/2018.

To represent an high risk scenario we have taken the worst wildfire in California, the __"Camp Fire"__.

Due to reconstruction this wildfire spreaded $10$ acres in the first $20$ minutes from ignition [^campfire], then it spread $5000$ acres in just $3$ hours.

So we can consider the "safe zone" just the first $20$ minutes and a little more. This gives us the first treshold sleep time.

Furthermore we want that the emergency aids arrives in time to contain the wildfire. We can estimate this to $15$ minutes [^emtime].

So we may set sleep time for high risk: $$\text{first\_10\_acres\_time} - \text{first\_responders\_arrival\_and\_setup} = 20m - 15m = 5m$$

### Other Risk Scenario

Finally we can estimate other two risk sleep time as:

__Medium Risk (MR):__ $$\text{HR\_sleep\_time} * \frac{HR\_fire\_prob}{MR\_fire\_prob} = 5m * \frac{3.08}{0.79} \simeq 20m$$

__Low Risk (LR):__ $$\text{HR\_sleep\_time} * \frac{HR\_fire\_prob}{LR\_fire\_prob} = 5m * \frac{3.08}{0.12} \simeq 120m$$

This concludes the __Duty Cycle Analysis__.

[^fwd]: [Frontline Wildfire Defense](https://www.frontlinewildfire.com/wildfire-news-and-resources/what-is-wildfire-suppression-and-can-it-save-your-home/)

[^novascotia]: [Nova Scotia](https://novascotia.ca/natr/forestprotection/wildfire/bffsc/lessons/lesson5/suppression.asp)

[^iere]: [iere](https://iere.org/how-fast-does-a-wildfire-spread/#Measuring_and_Predicting_Wildfire_Spread)

[^campfire]: [NIST](https://www.nist.gov/programs-projects/wildland-urban-interface-wui-fire-data-collection-parcel-vulnerabilities/nist/fire)

[^emtime]: [emergen](https://www.emergent.tech/blog/nfpa-1710-response-times)