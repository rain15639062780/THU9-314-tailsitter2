
include(common/px4_upload)

add_custom_target(sercon)
set_target_properties(sercon PROPERTIES
	PRIORITY "SCHED_PRIORITY_DEFAULT"
	MAIN "sercon"
	STACK_MAIN "2048"
	COMPILE_FLAGS "-Os")

add_custom_target(serdis)
set_target_properties(serdis PROPERTIES
	PRIORITY "SCHED_PRIORITY_DEFAULT"
	MAIN "serdis"
	STACK_MAIN "2048"
	COMPILE_FLAGS "-Os")

# builtin commands from module list
set(module_list)
list(APPEND module_list
	${module_libraries}
	${config_extra_builtin_cmds}
	)
list(SORT module_list)

set(builtin_apps_string)
set(builtin_apps_decl_string)
set(command_count 0)

foreach(module ${module_list})
	foreach(property MAIN STACK_MAIN PRIORITY)
		get_target_property(${property} ${module} ${property})
	endforeach()
	if (MAIN)
		set(builtin_apps_string "${builtin_apps_string}\t{\"${MAIN}\", ${PRIORITY}, ${STACK_MAIN}, ${MAIN}_main},\n")
		set(builtin_apps_decl_string "${builtin_apps_decl_string}extern int ${MAIN}_main(int argc, char *argv[]);\n")
		math(EXPR command_count "${command_count}+1")
	endif()
endforeach()

configure_file(${PX4_SOURCE_DIR}/cmake/nuttx/builtin_commands.c.in builtin_commands.c)

# add executable
set(fw_name ${CONFIG}.elf)
add_executable(${fw_name} builtin_commands.c)

set_target_properties(${fw_name} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${PX4_BINARY_DIR}
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PX4_BINARY_DIR}
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PX4_BINARY_DIR}
)

# build NuttX
add_subdirectory(${PX4_SOURCE_DIR}/NuttX ${PX4_BINARY_DIR}/NuttX)
add_dependencies(${fw_name} nuttx_build)

#
# Bootloaders use a different ld file, a separate startup file
# and need to override the os version of sched_process_timer
set(startup_libs)
set(nuttx_bootloader_wrappers)
if ("${nuttx_config_type}" STREQUAL "bootloader")
	set(nuttx_ld_prefix "bootloader")
	set(nuttx_bootloader_wrappers "-Wl,-wrap,sched_process_timer -Wl,-wrap,sem_post -Wl,-wrap,sem_wait")

	add_library(nuttx_startup ${PX4_SOURCE_DIR}/NuttX/nuttx/arch/arm/src/stm32/gnu/stm32_vectors.S)
	add_dependencies(nuttx_startup nuttx_arch_build)
	list(APPEND startup_libs nuttx_startup)
endif()

set(romfs_lib)
if (config_romfs_root)
	message(STATUS "ROMFS: ${config_romfs_root}")

	set(romfs_temp_dir ${PX4_BINARY_DIR}/ROMFS/${config_romfs_root})
	set(romfs_src_dir ${PX4_SOURCE_DIR}/ROMFS/${config_romfs_root})

	# directory setup
	# copy all romfs files, process airframes, prune comments
	file(GLOB_RECURSE init_airframes ${PX4_SOURCE_DIR}/ROMFS/${config_romfs_root}/*/[1-9]*)
	add_custom_command(OUTPUT ${romfs_temp_dir}/init.d/rcS ${romfs_temp_dir}/init.d/rc.autostart
		#COMMAND cmake -E remove_directory ${romfs_temp_dir}
		COMMAND cmake -E copy_directory ${romfs_src_dir} ${romfs_temp_dir}
		COMMAND ${PYTHON_EXECUTABLE} ${PX4_SOURCE_DIR}/Tools/px_process_airframes.py
			-a ${romfs_temp_dir}/init.d
			-s ${romfs_temp_dir}/init.d/rc.autostart
			--board ${BOARD}
		COMMAND ${PYTHON_EXECUTABLE} ${PX4_SOURCE_DIR}/Tools/px_romfs_pruner.py
			--folder ${romfs_temp_dir} --board ${BOARD}
		DEPENDS
			${romfs_src_files}
			${init_airframes}
			${PX4_SOURCE_DIR}/ROMFS/${config_romfs_root}/init.d/rcS
			${PX4_SOURCE_DIR}/Tools/px_process_airframes.py
		)

	set(romfs_dependencies)
	list(APPEND romfs_dependencies
		${romfs_temp_dir}/init.d/rcS
		${romfs_temp_dir}/init.d/rc.autostart
		)

	if (config_io_board)
		message(STATUS "ROMFS including IO: ${config_io_board}")

		set(fw_io_exe "${PX4_SOURCE_DIR}/build_${config_io_board}_default/nuttx_${config_io_board}_default.elf")
		set(fw_io_bin "${PX4_BINARY_DIR}/ROMFS/${config_romfs_root}/extras/${config_io_board}.bin")

		file(GLOB_RECURSE px4io_driver_files ${PX4_SOURCE_DIR}/src/drivers/boards/${config_io_board}/*)
		file(GLOB_RECURSE px4io_config_files ${PX4_SOURCE_DIR}/NuttX/configs/${config_io_board}/*)
		file(GLOB_RECURSE px4io_firmware_files ${PX4_SOURCE_DIR}/src/modules/px4iofirmware/*)
		file(GLOB_RECURSE px4io_rc_files ${PX4_SOURCE_DIR}/src/lib/rc/*)
		add_custom_command(OUTPUT ${fw_io_exe}
			COMMAND make --no-print-directory ${config_io_board}_default
			DEPENDS ${px4io_driver_files} ${px4io_config_files} ${px4io_firmware_files} ${px4io_rc_files}
			WORKING_DIRECTORY ${PX4_SOURCE_DIR}
			COMMENT "Building ${config_io_board}"
			USES_TERMINAL
			)
		add_custom_command(OUTPUT ${fw_io_bin}
			COMMAND mkdir -p ${PX4_BINARY_DIR}/ROMFS/${config_romfs_root}/extras/
			COMMAND ${OBJCOPY} -O binary ${fw_io_exe} ${fw_io_bin}
			DEPENDS
				${fw_io_exe}
				${romfs_temp_dir}/init.d/rcS
				${romfs_temp_dir}/init.d/rc.autostart
			COMMENT "Copying ${config_io_board} to ROMFS extras"
			)
		list(APPEND romfs_dependencies
			${fw_io_bin}
			)
	endif()

	# create romfs.bin
	add_custom_command(OUTPUT romfs.img
		COMMAND ${GENROMFS} -f romfs.img -d ${romfs_temp_dir} -V "NSHInitVol"
		DEPENDS ${romfs_dependencies}
		)

	# create romfs.o
	add_custom_command(OUTPUT romfs.o
		COMMAND ${OBJCOPY} -I binary -O elf32-littlearm -B arm romfs.img romfs.o
			--redefine-sym _binary_romfs_img_start=romfs_img
			--redefine-sym _binary_romfs_img_size=romfs_img_len
			--strip-symbol _binary_romfs_img_end
			--rename-section .data=.rodata
		DEPENDS romfs.img
		)

	add_library(romfs STATIC romfs.o)
	set_target_properties(romfs PROPERTIES LINKER_LANGUAGE C)

	set(romfs_lib romfs)
	add_dependencies(${fw_name} romfs)
endif()

# link firmware
set(ld_script "ld.script")
if (config_nuttx_ld_script)
	set(ld_script ${config_nuttx_ld_script})
endif()

set_target_properties(${fw_name} PROPERTIES LINK_FLAGS
	"-T${PX4_BINARY_DIR}/NuttX/nuttx/configs/${BOARD}/scripts/${nuttx_ld_prefix}${ld_script} -Wl,-Map=${PX4_BINARY_DIR}/${CONFIG}.map"
	)

target_link_libraries(${fw_name}
	-Wl,--warn-common
	-Wl,--gc-sections
	${nuttx_bootloader_wrappers}
	-Wl,--start-group

	${startup_libs}

	nuttx_apps
	nuttx_arch
	nuttx_binfmt
	nuttx_board
	nuttx_c
	nuttx_configs
	nuttx_cxx
	nuttx_drivers
	nuttx_fs
	nuttx_mm
	nuttx_sched

	${module_libraries}
	${df_driver_libs}
	${config_extra_libs}
	${romfs_lib}

	m
	gcc

	-Wl,--end-group
	)

# create px4 file (combined firmware and metadata)
#
# Bootloaders do not need .px4 or xml
if("${nuttx_config_type}" STREQUAL "nsh" AND NOT ${CONFIG} MATCHES "px4io")
	set(fw_file ${PX4_BINARY_DIR}/${CONFIG}.px4)

	add_custom_command(OUTPUT ${BOARD}.bin
		COMMAND ${OBJCOPY} -O binary ${PX4_BINARY_DIR}/${fw_name} ${BOARD}.bin
		DEPENDS ${fw_name}
		)

	add_custom_command(OUTPUT ${PX4_BINARY_DIR}/${CONFIG}.px4
		COMMAND ${PYTHON_EXECUTABLE} ${PX4_SOURCE_DIR}/Tools/px_mkfw.py
			--prototype ${PX4_SOURCE_DIR}/Images/${BOARD}.prototype
			--git_identity ${PX4_SOURCE_DIR}
			--parameter_xml ${PX4_BINARY_DIR}/parameters.xml
			--airframe_xml ${PX4_BINARY_DIR}/airframes.xml
			--image ${BOARD}.bin > ${fw_file}
		DEPENDS ${BOARD}.bin parameters_xml airframes_xml
		COMMENT "Creating ${CONFIG}.px4"
		)

	add_custom_target(px4 ALL DEPENDS ${PX4_BINARY_DIR}/${CONFIG}.px4)

	# upload helper
	if (${BOARD} STREQUAL "aerofc-v1")
		px4_add_upload_aero(OUT upload OS ${OS} BOARD ${BOARD} BUNDLE ${fw_file})
	else()
		px4_add_upload(OUT upload OS ${OS} BOARD ${BOARD} BUNDLE ${fw_file})
	endif()
endif()

# print size
add_custom_target(size
	COMMAND size ${fw_name}
	DEPENDS ${fw_name}
	WORKING_DIRECTORY ${PX4_BINARY_DIR}
	)

# print weak symbols
add_custom_target(check_weak
	COMMAND ${NM} ${fw_name} | ${GREP} " w " | cat
	DEPENDS ${fw_name}
	VERBATIM
	)

# debugger helpers
configure_file(gdbinit.in .gdbinit)

add_custom_target(debug
	COMMAND ${GDB} $<TARGET_FILE:${fw_name}>
	DEPENDS ${fw_name} ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit
	)

add_custom_target(debug_tui
	COMMAND ${GDBTUI} $<TARGET_FILE:${fw_name}>
	DEPENDS ${fw_name} ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit
	)

add_custom_target(debug_ddd
	COMMAND ${DDD} --debugger ${GDB} $<TARGET_FILE:${fw_name}>
	DEPENDS ${fw_name} ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit
	)

add_custom_target(debug_io
	COMMAND ${GDB} ${fw_io_path}
	DEPENDS ${fw_name} ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit
	)

add_custom_target(debug_io_tui
	COMMAND ${GDBTUI} ${fw_io_path}
	DEPENDS ${fw_name} ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit
	)

add_custom_target(debug_io_ddd
	COMMAND ${DDD} --debugger ${GDB} ${fw_io_path}}
	DEPENDS ${fw_io} ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit
	)
