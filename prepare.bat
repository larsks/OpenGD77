@echo OFF

if exist firmware\linkerdata (
	if exist firmware\tools\codec_cleaner.exe (
		cd firmware\linkerdata && ..\tools\codec_cleaner.exe -C
		cd ..\..
	) else (
		@echo Error: The required tools are not installed in firmware/tools, the process cannot be completed.
		exit /b 1
	)
) else (
	@echo Error: Your source tree is incomplete, please fix this.
	exit /b 1
)

exit /b 0
