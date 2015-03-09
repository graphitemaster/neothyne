# Color grading in Neothyne explained

### Understanding light and color

There are three basic components of visible light: Red, Green and Blue. We
call these colors RGB for short and they form the basis of something we call
an additive color scheme. We call it additive because adding different
combinations of Red, Green and Blue produce either Cyan, Magenta, Yellow or
White (all of them combined.) Cyan, Magenta and Yellow, or CYM for short is a
subtractive color scheme. The term subtractive comes from the fact that the
color we see in objects is reflected light minus any degree of Red, Green and
Blue. If something absorbs Green and Blue light, only to reflect Red, we see Red.
Whereas, if it only absorbs Red, it will reflect Blue and Green and the result
becomes Cyan. Thus, when you subtract various wavelengths through absorption, any
shade of any color can be produced.

It's also important to know what shadows, midtones and highlights are. Highlights
are the lightest areas of a scene, therefor the parts which have the most light
hitting it. If the scene has too many highlights, we usually say it's overexposed
and the scene is lacking in detail. Midtones show the middle tones of a scene -
the colors that are "in-between". For example, if we had a black and white
scene, the midtone would be gray - somewhere between the two. You want a generous
amount of midtone in a balanced scene but at the same time, you don't want
everything to be gray or flat (filmic tone mapping). Shadows are the darkest areas
that carry little light. A scene with too many shadows may be underexposed and
will not show much detail. Scenes with a lot of shadows and highlights and very
few midtones have high contrast - the effect is dramatic, especially in scenes
with lots of black and white.

### Understanding color/white balance

Sometimes getting colors to look correct and accurate as possible may require
adjustment to the white balance (as well as color balance). You may have
noticed that it's sometimes hard to achieve a normal or abnormal look to the scene
due to the way difference sources of light have different colors or temperatures
to them. You may want to make lights appear more florescent or more incandescent
for instance.

The range in different temperatures ranges from the very cool light blue sky
straight through to a very warm light like that of a candle. We don't normally
notice this difference in temperature because our eyes adjust automatically for
it. So, unless the temperature is very extreme, everything will generally look
the same to us under differing lighting conditions. Making these adjustments
automatically for rendering is just not feasible so you may need to tell the
engine how you want to treat different light, or you may just want to manipulate
white balance for artistic effect too.

The way the color balance tool works in the Neothyne's color grading options is
in terms of a ratio between two colors. For instance, if you turn up both
the Blue and the Green, this will effectively reduce the Red. Similarly, if you
go all the way towards Yellow and Magenta, then you increase the Red. To understand
why this is we have to first explain some color theory.

Back in 1931, William David Wright and John Guild created what we now know today
as the CIE XYZ color space. Derived from a series of experiments done by those two
in the 1920s they discovered the following facts about color. In our field of
vision, Green is represented in the widest range; Magenta is the opposite color.
Blue represents the remaining range of wavelengths in our field of vision, also
balanced by Yellow as the opposite color. There is also 90 degrees from Green and
Magenta on the color wheel. From these two alone we can identify any color on
the wheel (as well as some imaginary ones.)

Realize now, that each of these colors has a direct or indirect effect on one
another. If one area of the current scene is too Green and you pull it out, the
rest of the scene will become more Magenta (Green's counterpart.) This may be
fine if the entire scene has too much of a Green cast as the correction serves
to balance out the color.

The color balance tool in Neothyne also has the option to preserve luminosity when
balancing color. This options serves to preserve the apparent brightness in the
scene when adjusting midtones for instance.

### Understanding Hue, Saturation and Lightness

Lets define the terminology first. A Hue is synonymous with a color, that is to
say Hues are colors. When we say something is Red, Green or Blue, what we're
conveying is the Hue. Saturation on the other hand refers to how a Hue appears
under particular lighting conditions. Think of Saturation in terms of weak vs
strong or pale vs pure. Finally, Lightness refers to how light or dark a color is.
Lighter colors have higher values. For example, Orange has a higher value than
navy Blue or dark Purple. Black has the lowest value of any Hue, and White the
highest.

You also have the option to specify how much color ranges will overlap. The
effect of this is very subtle and will only work on colors next to each other
(or close to each other.) The more overlap, the more closer colors begin to
become the same Hue.

### Understanding Brightness and Contrast

Brightness and Contrast are simple concepts. What they affect however is not as
obvious. When applying a Brightness adjustment in a negative direction you're
decreasing tonal value while expanding highlights in the scene. The opposite
direction does the opposite; increase tonal value while expanding shadows.
Contrast, also a simple concept, merely expands or shrinks this overall range
of tonal values.

It's important to note that Brightness and Contrast apply a linear adjustment
to the scene, i.e it only shifts pixel values higher or lower. This can cause
clipping as well as loss of scene detail, especially in highlight or shadow
areas.

### Putting it all together

In Neothyne color grading is performed in the following steps: "Color balance",
"Hue and Saturation" and "Brightness and Contrast", in that order respectively.

The key to using any of this is to play around with the settings and see what
works and what doesn't. Adjust in small steps rather than dramatic ones, as big
changes and edits are rarely flattering.

It's generally advised that when working with Hue and Saturation not to use
the Lightness option to adjust exposure, it may look like it does a nice job, but
it doesn't. If you, for example, darken a scene using lightness, you'll begin to
lose any areas defined as White.
