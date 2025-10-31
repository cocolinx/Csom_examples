# Copyright (c) 2023 cocolinx
# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=nRF9151_xxCA" "--speed=4000")
include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
