TEMPLATE = subdirs
SUBDIRS = svcore svgui svapp sub_vect

sub_vect.file = vectapp.pro

svgui.depends = svcore
svapp.depends = svcore svgui
sub_vect.depends = svcore svgui svapp
