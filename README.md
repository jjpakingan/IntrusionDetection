
# Multi-Function Intruder and Flood Detection

Created by Jeff Pakingan

Environment: Arduino

This is a personal project I developed. This has the following features:

 - Flood/Water Detection
 - Laser Detection
 - Trip Wire Detection
 - PIR Detection

Design Methodologies:

 - Used an ultra-lightweight scheduler using TThread to achieve concurrency.
 - "Program to interface not to an implementation" principle. Used polymorphism via abstract class/pure virtual functions to achieve flexibility.
