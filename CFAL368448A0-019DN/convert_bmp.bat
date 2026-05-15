@echo on
REM Set input and output filenames here, or leave blank to process all JPGs
REM set INPUT=your_input.jpg
set INPUT=
set OUTPUT=your_output.bmp

if not "%INPUT%"=="" (
    python3 convert_bmp.py "%INPUT%" "%OUTPUT%"
) else (
    for %%f in (*.jpg) do (
        python3 convert_bmp.py "%%f" "%%~nf.bmp"
    )
    for %%f in (*.png) do (
        python3 convert_bmp.py "%%f" "%%~nf.bmp"
    )
)

pause