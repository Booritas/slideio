---
layout: page
title: Python API
sidebar_link: true
sidebar_sort_order: 100
---
# Overview
The python module provides 2 python classes: Slide and Scene. Slide is a container object returned by the module function open_slide. In the simplest case, a Slide object contains a single Scene object. Some slides can contain multiple scenes. For example, a czi file can contain several scanned regions, each of them is represented as a Scene object. Scene class provides methods to access image pixel values and metadata.
See [Sphinx generated SlideIO python API](https://booritas.github.io/slideio/sphinx/)

# Installation

Installation
Installation of the modile available through pip.

{% gist 0c71e368e849d707a2834f8e2905bc9e %}

# Quick Start

Here is an example of a reading of a czi file:

{% gist 89c29934ebb371a60afdfd7821b9741f %}