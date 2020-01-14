@echo off
if "%1"=="" goto main

cd %1
type %1.doc | ..\..\addcr
set ASCMD=@asflags
..\..\asl -i ..\..\include -L +t 31 %1.asm
set ASCMD=
..\..\p2bin -k -l 0 -r $-$ %1
..\..\bincmp %1.bin %1.ori
if errorlevel 1 goto errcond
echo Test %1 succeeded!
set SUMPASS=%SUMPASS%!
echo %1 : OK >> ..\..\testlog
:goon
echo +---------------------------------------------------------------+
type %1.lst | find "assembly" >> ..\..\testlog
type %1.lst | find "Assemblierzeit" >> ..\..\testlog
if exist %1.lst del %1.lst >nul
if exist %1.inc del %1.inc >nul
if exist %1.bin del %1.bin >nul
cd ..
goto end

:errcond
echo Test %1 failed!
set SUMFAIL=%SUMFAIL%-
echo %1 : failed >> ..\..\testlog
goto goon

:main
if exist ..\addcr.exe goto nocomp
bcc -e..\addcr.exe -ml ..\addcr.c
del addcr.obj
:nocomp
if exist ..\bincmp.exe goto nocomp2
bcc -e..\bincmp.exe -ml ..\bincmp.c
del bincmp.obj
:nocomp2
echo executing self tests...
echo ================================================================= >..\testlog
echo Summaric results: >> ..\testlog
set SUMPASS=
set SUMFAIL=
call testall t_166
call testall t_16c5x
call testall t_16c84
call testall t_17c42
call testall t_1750
call testall t_1802
call testall t_1805
call testall t_251
call testall t_2650
call testall t_296
call testall t_29k
call testall t_32
call testall t_3201x
call testall t_3202x
call testall t_3203x
call testall t_3204x
call testall t_3205x
call testall t_3206x
call testall t_3254x
call testall t_370
call testall t_4004
call testall t_403
call testall t_4500
call testall t_47c00
call testall t_48
call testall t_56000
call testall t_56300
call testall t_65
call testall t_6502u
call testall t_65c19
call testall t_6804
call testall t_68040
call testall t_6805
call testall t_6808
call testall t_6812
call testall t_6816
call testall t_68kaddr
call testall t_68kdis
call testall t_68rs08
call testall t_7000
call testall t_75k0
call testall t_75xx_a
call testall t_75xx_b
call testall t_7700
call testall t_7720
call testall t_77230
call testall t_7725
call testall t_78c1x
call testall t_78k0
call testall t_78k2
call testall t_78k3
call testall t_78k4
call testall t_8008
call testall t_807x
call testall t_80c382
call testall t_821
call testall t_85
call testall t_870c
call testall t_87c800
call testall t_8X30x
call testall t_9331
call testall t_9331_1
call testall t_9331_2
call testall t_96
call testall t_960
call testall t_97c241
call testall t_9900
call testall t_ace
call testall t_adrstr
call testall t_avr
call testall t_bas52
call testall t_bsr
call testall t_buf32
call testall t_cmdcpu
call testall t_cop4
call testall t_cop8
call testall t_dc
call testall t_dx
call testall t_ez8
call testall t_f2mc8l
call testall t_f2mc16
call testall t_fl90
call testall t_fl900
call testall t_full09
call testall t_kcpsm
call testall t_kcpsm3
call testall t_h8_3
call testall t_h8_5
call testall t_hcs08
call testall t_hmcs4x
call testall t_huc6280
call testall t_longline
call testall t_m16
call testall t_m16c
call testall t_macargs
call testall t_mcore
call testall t_mic51
call testall t_mico8
call testall t_msp
call testall t_msp430x
call testall t_nestedstructs
call testall t_olms40
call testall t_olms50
call testall t_oki48
call testall t_parsys
call testall t_s12x
call testall t_scmp
call testall t_secdri
call testall t_shift
call testall t_st6
call testall t_st7
call testall t_st9
call testall t_stringify
call testall t_structs
call testall t_tmpsym
call testall t_tms1000
call testall t_tms1100
call testall t_tms7
call testall t_vector
call testall t_w65c02s
call testall t_xa
call testall t_xcore
call testall t_xgate
call testall t_z380
call testall t_z8
call testall t_z80syntax
echo successes: %SUMPASS% >> ..\testlog
echo failures: %SUMFAIL% >> ..\testlog
type ..\testlog

:end
