TEMPLATE = subdirs
SUBDIRS = sub_bq sub_dataquay svcore svgui svapp sub_vect

sub_bq.file = bq.pro
sub_vect.file = vectapp.pro

sub_dataquay.file = dataquay/lib.pro

svgui.depends = svcore
svapp.depends = svcore svgui
sub_vect.depends = svcore svgui svapp
