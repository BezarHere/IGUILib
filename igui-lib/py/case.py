

import re


s = """None = 0,
ReferenceBox, // <- unfilled box
Panel,

// buttons
Button,
RadialButton,
Checkbox, // [X] or [_]
CheckButton, // [1--] or [--O]
DropdownMenu, // <- dropdown button, labels and sliders
DropdownOption, // <- dropdown selection

// ranges
HSlider,
VSlider,
HProgressBar,
VProgressBar,
HScrollbar,
VScrollbar,

Sprite,
Line,
SolidColor,
Label,
TextInput,
VSeprator,
HSeprator"""

COMMENTS = re.compile('//.*?\n')


s = COMMENTS.sub('\n', s)


svals = tuple(i.strip() for i in s.split(','))
out = ''

for i in svals:
	out += f"case NodeType::{i}:" "\n{\n\t\n}\nbreak;\n"
print(out)
