#!/usr/bin/env bash
set -u

# Runs all generated E2E tests under:
#   sample_kharuya/expect_success
#   sample_kharuya/expect_failure
#   sample_kharuya/rfc
#
# Each test directory is expected to contain a single .cpp that:
# - starts ./webserv with ./webserv.conf (in that directory)
# - connects to listen port parsed from webserv.conf
# - sends a raw HTTP request, validates status code
#
# Usage examples:
#   bash sample_kharuya/run_all_tests.sh
#   bash sample_kharuya/run_all_tests.sh --keep-going
#   bash sample_kharuya/run_all_tests.sh --keep-bin
#   bash sample_kharuya/run_all_tests.sh --filter host

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

KEEP_GOING=0
KEEP_BIN=0
FILTER=""
TIMEOUT_SEC=20
ALLOW_DARWIN=0
CATEGORY_ONLY=""
SILENT=0

# Colors (enabled only when stdout is a TTY)
RED=""
GREEN=""
YELLOW=""
BOLD=""
RESET=""

print_usage()
{
	cat <<'USAGE'
Usage: run_all_tests.sh [options]

Options:
  --keep-going       Continue even if a test fails
  --keep-bin         Keep compiled test binaries in each directory
	--only <cat>        Run only one category: expect_success|expect_failure|rfc
	--success-only      Same as: --only expect_success
	--failure-only      Same as: --only expect_failure
	--rfc-only          Same as: --only rfc
	--allow-darwin      Attempt to run on macOS (not supported by default)
  --filter <substr>  Only run tests whose directory path contains <substr>
	--timeout <sec>    Per-test timeout if `timeout` exists (default: 20)
  --silent           Show only failed logs and summary
  -h, --help         Show this help
USAGE
}

while [ $# -gt 0 ]; do
	case "$1" in
		--keep-going)
			KEEP_GOING=1
			shift
			;;
		--allow-darwin)
			ALLOW_DARWIN=1
			shift
			;;
		--only)
			if [ $# -lt 2 ]; then
				echo "ERROR: --only requires an argument" >&2
				exit 2
			fi
			CATEGORY_ONLY="$2"
			shift 2
			;;
		--success-only)
			CATEGORY_ONLY="expect_success"
			shift
			;;
		--failure-only)
			CATEGORY_ONLY="expect_failure"
			shift
			;;
		--rfc-only)
			CATEGORY_ONLY="rfc"
			shift
			;;
		--keep-bin)
			KEEP_BIN=1
			shift
			;;
		--filter)
			if [ $# -lt 2 ]; then
				echo "ERROR: --filter requires an argument" >&2
				exit 2
			fi
			FILTER="$2"
			shift 2
			;;
		--timeout)
			if [ $# -lt 2 ]; then
				echo "ERROR: --timeout requires an argument" >&2
				exit 2
			fi
			TIMEOUT_SEC="$2"
			shift 2
			;;
		--silent)
			SILENT=1
			shift
			;;
		-h|--help)
			print_usage
			exit 0
			;;
		*)
			echo "ERROR: unknown option: $1" >&2
			print_usage >&2
			exit 2
			;;
	esac
done

if [ -n "${CATEGORY_ONLY}" ]; then
	case "${CATEGORY_ONLY}" in
		expect_success|expect_failure|rfc) ;;
		*)
			echo "ERROR: invalid category for --only: ${CATEGORY_ONLY}" >&2
			echo "       allowed: expect_success | expect_failure | rfc" >&2
			exit 2
			;;
	esac
fi

host_os=$(uname -s 2>/dev/null || echo "")
if [ "${host_os}" = "Darwin" ] && [ ${ALLOW_DARWIN} -eq 0 ]; then
	echo "ERROR: This E2E runner expects a Linux environment where ./webserv can be built/run (epoll)." >&2
	echo "       You're on macOS (Darwin). Run this script inside your Linux VM/container (e.g. OrbStack Ubuntu)." >&2
	echo "       If you know your build supports macOS, re-run with: --allow-darwin" >&2
	exit 2
fi

if [ -t 1 ]; then
	if command -v tput >/dev/null 2>&1; then
		# tput might fail if TERM is not set; ignore failures.
		RED=$(tput setaf 1 2>/dev/null || true)
		GREEN=$(tput setaf 2 2>/dev/null || true)
		YELLOW=$(tput setaf 3 2>/dev/null || true)
		BOLD=$(tput bold 2>/dev/null || true)
		RESET=$(tput sgr0 2>/dev/null || true)
	else
		RED=$'\033[31m'
		GREEN=$'\033[32m'
		YELLOW=$'\033[33m'
		BOLD=$'\033[1m'
		RESET=$'\033[0m'
	fi
fi

if [ ! -x "${REPO_ROOT}/webserv" ]; then
	echo "INFO: ${REPO_ROOT}/webserv not found. Building with make..." >&2
	( cd "${REPO_ROOT}" && make )
else
	# If the binary exists but is for a different OS/arch (e.g. ELF on macOS),
	# the generated tests will fail with "Exec format error" when they try to execve().
	need_rebuild=0
	if command -v file >/dev/null 2>&1; then
		fmt=$(file "${REPO_ROOT}/webserv" 2>/dev/null || echo "")
		if [ "${host_os}" = "Darwin" ]; then
			case "${fmt}" in
				*ELF*) need_rebuild=1 ;;
			esac
		elif [ "${host_os}" = "Linux" ]; then
			case "${fmt}" in
				*Mach-O*) need_rebuild=1 ;;
			esac
		fi
	else
		# Fallback: try executing the binary; exit code 126 often indicates exec format issues.
		"${REPO_ROOT}/webserv" >/dev/null 2>&1
		rc=$?
		if [ ${rc} -eq 126 ]; then
			need_rebuild=1
		fi
	fi
	if [ ${need_rebuild} -eq 1 ]; then
		echo "INFO: ${REPO_ROOT}/webserv seems incompatible with this OS (${host_os}). Rebuilding..." >&2
		( cd "${REPO_ROOT}" && make re )
	fi
fi

if [ ! -x "${REPO_ROOT}/webserv" ]; then
	echo "ERROR: ${REPO_ROOT}/webserv is still missing after build" >&2
	exit 1
fi

has_timeout=0
if command -v timeout >/dev/null 2>&1; then
	has_timeout=1
fi

compile_one()
{
	# $1: test_dir
	local test_dir="$1"
	local cpp
	cpp=$(ls -1 "${test_dir}"/*.cpp 2>/dev/null | head -n 1)
	if [ -z "${cpp}" ]; then
		return 10
	fi
	local bin="${test_dir}/.run_test"
	(
		cd "${test_dir}" && \
		c++ -std=c++98 -Wall -Wextra -Werror "$(basename "${cpp}")" -o "$(basename "${bin}")"
	)
	return $?
}

run_one()
{
	# $1: test_dir
	local test_dir="$1"
	local bin="${test_dir}/.run_test"
	if [ ! -x "${bin}" ]; then
		return 11
	fi
	if [ ${has_timeout} -eq 1 ]; then
		( cd "${test_dir}" && timeout "${TIMEOUT_SEC}" "./$(basename "${bin}")" )
		return $?
	fi
	( cd "${test_dir}" && "./$(basename "${bin}")" )
	return $?
}

if [ -n "${CATEGORY_ONLY}" ]; then
	categories=("${CATEGORY_ONLY}")
else
	categories=("expect_success" "expect_failure" "rfc")
fi

total=0
passed=0
failed=0
skipped=0

# Gather test directories (flat: category/*)
TEST_DIRS=()
for cat in "${categories[@]}"; do
	base="${SCRIPT_DIR}/${cat}"
	if [ ! -d "${base}" ]; then
		continue
	fi
	while IFS= read -r d; do
		TEST_DIRS+=("$d")
	done < <(find "${base}" -mindepth 1 -maxdepth 1 -type d | sort)
done

if [ ${#TEST_DIRS[@]} -eq 0 ]; then
	echo "ERROR: no test directories found under ${SCRIPT_DIR}" >&2
	exit 1
fi

for test_dir in "${TEST_DIRS[@]}"; do
	# optional filter
	if [ -n "${FILTER}" ]; then
		case "${test_dir}" in
			*"${FILTER}"*) ;; 
			*) continue ;; 
		esac
	fi

	total=$((total + 1))
	rel="${test_dir#${REPO_ROOT}/}"

	if [ ${SILENT} -eq 0 ]; then
		printf "%s=== [%s] %s ===%s\n" "${BOLD}" "${total}" "${rel}" "${RESET}"
	fi

	if [ ${SILENT} -eq 1 ]; then
		compile_out=$(compile_one "${test_dir}" 2>&1)
		c_rc=$?
	else
		compile_one "${test_dir}"
		c_rc=$?
	fi

	if [ ${c_rc} -ne 0 ]; then
		if [ ${c_rc} -eq 10 ]; then
			if [ ${SILENT} -eq 0 ]; then
				printf "%sSKIP%s: no .cpp found\n" "${YELLOW}" "${RESET}"
			fi
			skipped=$((skipped + 1))
			continue
		fi
		if [ ${SILENT} -eq 1 ]; then
			printf "%s=== [%s] %s ===%s\n" "${BOLD}" "${total}" "${rel}" "${RESET}"
			if [ -n "${compile_out}" ]; then
				echo "${compile_out}"
			fi
		fi
		printf "%sFAIL%s: compile error (rc=%s)\n" "${RED}" "${RESET}" "${c_rc}"
		failed=$((failed + 1))
		if [ ${KEEP_GOING} -eq 0 ]; then
			break
		fi
		continue
	fi

	if [ ${SILENT} -eq 1 ]; then
		run_out=$(run_one "${test_dir}" 2>&1)
		rc=$?
	else
		run_one "${test_dir}"
		rc=$?
	fi

	if [ ${rc} -eq 0 ]; then
		if [ ${SILENT} -eq 0 ]; then
			printf "%sPASS%s\n" "${GREEN}" "${RESET}"
		fi
		passed=$((passed + 1))
	else
		if [ ${SILENT} -eq 1 ]; then
			printf "%s=== [%s] %s ===%s\n" "${BOLD}" "${total}" "${rel}" "${RESET}"
			if [ -n "${run_out}" ]; then
				echo "${run_out}"
			fi
		fi
		if [ ${has_timeout} -eq 1 ] && [ ${rc} -eq 124 ]; then
			printf "%sFAIL%s: timeout (%ss)\n" "${RED}" "${RESET}" "${TIMEOUT_SEC}"
		else
			printf "%sFAIL%s: test exited with rc=%s\n" "${RED}" "${RESET}" "${rc}"
		fi
		failed=$((failed + 1))
		if [ ${KEEP_GOING} -eq 0 ]; then
			if [ ${KEEP_BIN} -eq 0 ]; then
				rm -f "${test_dir}/.run_test" >/dev/null 2>&1 || true
			fi
			break
		fi
	fi

	if [ ${KEEP_BIN} -eq 0 ]; then
		rm -f "${test_dir}/.run_test" >/dev/null 2>&1 || true
	fi

done

echo ""
printf "%s=== Summary ===%s\n" "${BOLD}" "${RESET}"
printf "total:   %s\n" "${total}"
printf "passed:  %s%s%s\n" "${GREEN}" "${passed}" "${RESET}"
if [ ${failed} -ne 0 ]; then
	printf "failed:  %s%s%s\n" "${RED}" "${failed}" "${RESET}"
else
	printf "failed:  %s\n" "${failed}"
fi
printf "skipped: %s%s%s\n" "${YELLOW}" "${skipped}" "${RESET}"

if [ ${failed} -ne 0 ]; then
	exit 1
fi
exit 0
