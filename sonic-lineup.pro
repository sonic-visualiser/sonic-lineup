
TEMPLATE = subdirs

SUBDIRS += \
        sub_base

win32-msvc* {
    SUBDIRS += \
        sub_os_other \
        sub_os_win10
}

# We build the tests on every platform, though at the time of
# writing they are only automatically run on non-Windows platforms
# (because of the difficulty of getting them running nicely in the
# IDE without causing great confusion if a test fails).
SUBDIRS += \
        sub_test_svcore_base \
        sub_test_svcore_system \
        sub_test_svcore_data_fileio \
        sub_test_svcore_data_model

SUBDIRS += \
	checker \
	sub_server \
        sub_convert \
        sub_match \
        sub_tuning_difference \
        sub_pyin \
        sub_nnls_chroma \
        sub_qm_vamp_plugins \
        sub_azi \
	sub_vect

sub_base.file = base.pro

sub_os_other.file = os-other.pro
sub_os_win10.file = os-win10.pro

sub_test_svcore_base.file = test-svcore-base.pro
sub_test_svcore_system.file = test-svcore-system.pro
sub_test_svcore_data_fileio.file = test-svcore-data-fileio.pro
sub_test_svcore_data_model.file = test-svcore-data-model.pro

sub_server.file = server.pro
sub_convert.file = convert.pro
sub_match.file = match.pro
sub_pyin.file = pyin.pro
sub_tuning_difference.file = tuning-difference.pro
sub_nnls_chroma.file = nnls-chroma.pro
sub_qm_vamp_plugins.file = qm-vamp-plugins.pro
sub_azi.file = azi.pro
sub_vect.file = vectapp.pro

CONFIG += ordered
