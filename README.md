![pedal top view](https://github.com/dbostian/expressyourself/blob/main/artwork/exp1.jpg)

# express yourself

A DIY triple expression pedal. This pedal was heavily inspired by the Old Blood Noise Endeavors Expression Ramper x3. I wanted to add the ability for the pedal to modulate its own values. It has three expression outputs that can sync with each other, and affect each other in weird ways.

This project is work in progress, and is released on an as-is basis. This repository contains all code, artwork (I printed a vinyl sticker), pcb schematics, gerbers, a bom, as well as 3d printing files (stls, f3d, step) for a 3d printed enclosure. The pedal will fit in a 125b enclosure. The code is working as described below/shown in the videos, but is in a preliminary stage and may contain bugs. In short, use at your own risk.

I am not planning to release detailed build instructions, but if you're a steady hand at soldering, have built a few diy pedals before, ordered custom pcbs, sourced a bom, and have flashed an arduino or two, you should be able to handle this.

Overview Video: https://www.youtube.com/watch?v=8aHI_HfOTI8
Followup Video: https://www.youtube.com/watch?v=umGa9XkKtoM
Part 3: https://www.youtube.com/watch?v=ajVojKqwqVI

## basic usage

The pedal has three sets of expression controls, arranged in columns from left to right. For any exp, you can set the wave **shape** (triangle, square or sine), an **a** value and a **b** value, and a **rate**. The exp will oscillate between **a** and **b** at the **rate** you set, using the selected **shape**. All knob settings are relative - the knob may not match the value when changing the **set** switch, so turning the knob will raise or lower the value accordingly. Pay attention to the LED's brightness, as it reflects the actual value.

Exp 1 can also have its **rate** determined by tap tempo. At most, the last five taps will be used.

To enable an exp's jack, tap the footswitch to enable it. The LED will turn white. Tap again to turn it off. The output of a jack is:
* A if the set switch is on **a**
* B if the set switch is on **b**
* Oscillating if the set switch is on **rate** and the exp is **on**
* A if the set switch is on **rate** and the exp is **off**

You can sync exp2 and exp3 the exp to their left. If **sync** is set to **rate**, the **rate** value will be copied over. If set to **all**, the **a** and **b** values will be copied as well. When syncing (**rate** or **all**), changing the **rate** value will adjust the local rate in multiples of the exp to the left - 1/8, 1/4, 1/2, 1x, 2x, 3x, or 4x.

If you hold down any footswitch while changing the **sync** switch, the exp will enter **mod** mode. The output of the exp to the left will control an aspect of this exp. as follows:
* In **amp** mod mode, the amplitude of the wave will be changed by the output of the exp to the left. **a** and **b** will move apart or closer together about their midpoint. 0% touching, 50% original values, 100% twice as far apart.
* In **freq** mod mode, the frequency of the wave will be changed. The **rate** value will be replaced with the output of exp to the left.
* In **phase** mod mode, the phase of the wave will be offset by the value of the exp to the left. 0%: minus one half wavelength, 50%: no change, 100%: plus one half wavelength.

In **amp** mod mode, the value may exceed the maximum or minimum value of the pedal. In this case, **wavefolding** will occur.

Changing the **sync** switch without holding a footswitch down will turn off the **mod** mode. You can have **sync** mode and **mod** mode active at the same time: move the **sync** switch to the desired **sync** mode, hold the foot switch and select the desired **mod** mode. You cannot have more than one **sync** mode or **mod** mode active at the same time.

If you change the **shape** switch while holding a foot switch, you will enter **random** mode - each time the wave completes a cycle, random values for **a** and **b** will be chosen, using values between the original **a** and **b**.

Future features and code changes:
* One-shot mode. Holding the tap tempo button while selecting shape will put it into one-shot mode. The EXP will move from A to B when the footswitch is tapped, and back from B to A again when tapped again, and so on.
* Hold to pause. While a footswitch is held, the exp's output and progress will be paused. It will resume when the footswitch is released.

![schematic](https://github.com/dbostian/expressyourself/blob/main/artwork/schematic.jpg)

## design notes

The schematic and pcb were layed out in KiCad 10.

This circuit uses every IO pin available on the Arduino Pro Micro. To reduce the number of pins used for the switches (many of which have three positions), I used a key matrix with diodes arranged in a 4 x 5 grid. This reduced the pins needed to 9. This is very similar to how a computer keyboard works. For each three-way switch, I can check two positions, and if neither is "pressed," I can tell that it is in the middle position. I'm using the Adafruit_Keypad library for this. They switch matrix handles all of the toggle switches and the momentaries (tap tempo, plus the foot switches). For more information on how a key matrix works (and why this pedal needs so many diodes), check out this article: https://www.dribin.org/dave/keyboard/one_html/

The digipots are controlled using a software SPI. In using every pin available, I ended up using the MISO pin as one of the CS lines. The built in SPI library conflicted with that use case, hence the bit-banging solution.

The artwork is based on a photo of a cloud that I took, converted to halftones. This is color shifted using a CMYK color scheme for the three expression units. I created the art in InkScape.

The 3d print files were for testing hole location, prior to drilling into metal. I've included them for reference. These were designed in Autodesk Fusion.

The code was written in the Arduino IDE. AI use in this project is limited to GitHub copilot commit messages from editing this README file - not for code generation, pcb design, art, or anything else. It certainly didn't help with soldering.

![pcbs](https://github.com/dbostian/expressyourself/blob/main/artwork/exp5.jpg)
![soldered componentst](https://github.com/dbostian/expressyourself/blob/main/artwork/exp6.jpg)
![3d printed enclosure testing](https://github.com/dbostian/expressyourself/blob/main/artwork/exp7.jpg)

## build notes

The expressionx3.csv contains a fairly complete parts list. Pay attention to the csv, not what is in KiCad.

* JST connectors did not fit - ignore bom and just use pin header.
* TRS jack choice is important. Others did not fit in the enclosure.
* The pcb is challenging to solder, as I completely disregarded courtyards during design. Plan ahead while soldering. Paying for assembly probably won't work without some updates.
* I installed the pro micro using pin header for clearance. You may want to drill a hole/mill a slot in the side of the enclosure for easy usb cable access, especially if you are planning on experimenting with your own code.
* You may need to bend some terminals during assembly to get everything to fit inside. I needed this for the foot switches.
* You might need to use other sized digipots from the MCP41xx series (bigger/smaller than 50k), depending on what you are plugging them into.
* Someone suggested I flip the outer two foot switches around to make it possible to hit the center switch with normal sized toes.

![pedal gutshot](https://github.com/dbostian/expressyourself/blob/main/artwork/exp2.jpg)
![pedal side view](https://github.com/dbostian/expressyourself/blob/main/artwork/exp3.jpg)
![pedal top](https://github.com/dbostian/expressyourself/blob/main/artwork/exp4.jpg)

