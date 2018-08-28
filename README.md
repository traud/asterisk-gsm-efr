# Asterisk patch for GSM-EFR

Asterisk already supports Full Rate ([GSM-FR](http://tools.ietf.org/html/rfc3551#section-4.5.8)). This patch adds Enhanced Full Rate ([GSM-EFR](http://tools.ietf.org/html/rfc3551#section-4.5.9)).

## Installing the patch

At least Asterisk 13 is required. These changes were last tested with Asterisk 13.22 (and Asterisk 16.0). If you use a newer version and the patch fails, please, [report](https://help.github.com/articles/creating-an-issue/)!

    cd /usr/src/
    wget downloads.asterisk.org/pub/telephony/asterisk/asterisk-13-current.tar.gz
    tar zxf ./asterisk*
    cd ./asterisk*
    sudo apt --no-install-recommends --assume-yes install autoconf automake build-essential pkg-config libedit-dev libjansson-dev libsqlite3-dev uuid-dev libxslt1-dev xmlstarlet

Install library:

If you do not want transcoding but pass-through only (because of license issues) please, skip this step. To support transcoding, you’ll need to install OpenCORE AMR, for example in Debian/Ubuntu:

    sudo apt --assume-yes install libopencore-amrnb-dev

The patch relies on [my AMR patch](http://github.com/traud/asterisk-amr). Therefore, you have to apply one of those patches as well:

    wget github.com/traud/asterisk-amr/archive/master.zip
    unzip -qq master.zip
    rm master.zip
    cp --verbose --recursive ./asterisk-amr*/* ./
    patch -p0 <./build_tools.patch
    wget github.com/traud/asterisk-gsm-efr/archive/master.zip
    unzip -qq master.zip
    rm master.zip
    cp --verbose --recursive ./asterisk-gsm-efr*/* ./
    patch -p0 <./codec_gsm_efr.patch

Run the bootstrap script to re-generate configure:

    ./bootstrap.sh

Configure your patched Asterisk:

    ./configure

Compile and install:

    make
    sudo make install

## Testing
You can test GSM-EFR with the VoIP/SIP client in Google Android (AOSP): All apps → Phone → Options → Settings → Calls → Calling accounts → SIP accounts. I recommend Google Nexus devices because some manufactures do not include that client. Even with those manufactures who include the client, some do not test the built-in VoIP/SIP correctly. Therefore, you might experience degraded voice quality. GSM-EFR is the highest AMR mode actually. A paper for [ICASSP 2010](http://research.nokia.com/files/public/%5B11%5D_ICASSP2010_Voice%20Quality%20Evaluation%20of%20Various%20Codecs.pdf) compared several narrow-band codecs. Therefore, please, consider [AMR](http://github.com/traud/asterisk-amr).

## Thanks goes to
* teams of the Android Open Source Project (AOSP), OpenCORE AMR, Debian Multimedia, and Ubuntu for providing the library.
* Asterisk team: Thanks to their efforts and architecture the module was written in one working day.
* [Юрий Остапчук](http://code.google.com/p/fs-mod-opencore-amr/source/browse/mod_opencore_amr/mod_opencore_amr.c) provided the starting point with his code for FreeSWITCH.