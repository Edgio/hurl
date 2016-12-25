#!/bin/sh
rm -f _libhlx.mri
echo "create libhlx.a" >> _libhlx.mri
echo "addlib $1/ext/udns-0.4/libudns.a" >> _libhlx.mri
echo "addlib libhlxcore.a" >> _libhlx.mri
echo "save" >> _libhlx.mri
echo "end" >> _libhlx.mri
