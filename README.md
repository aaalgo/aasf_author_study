AASF Author Study
=================

# 1. Introduction

This project is to reproduce the analysis done by Xie et al. [1]

# 2. Processing

## 2.1 Data Setup

It is assumed that the OpenAlex data is stored in `data/authors` (Only
the authors subset is used.)

## 2.2 Preprocessing & Counting

The main processing code is implemented in C++ (with the help of AI) so
it can be run quickly on a single machine.

```
# Step 1. filtering.
# This step filter the data and put relevant
# data for downstream processing in data/filtered.
./run filter

# Step 2.
# This step does the counting.
./run count
```


# Reference

- [1] Yu Xie, Xihong Lin, Ju Li, Qian He, Junming Huang. Caught in the Crossfire: Fears of Chinese-American Scientists. PNAS 2023.
