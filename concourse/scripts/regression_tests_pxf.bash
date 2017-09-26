#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"
GPHOME="/usr/local/greenplum-db-devel"
PXF_HOME="${GPHOME}/pxf"

function run_regression_test() {
	cat > /home/gpadmin/run_regression_test.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source ${GPHOME}/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux"
	source gpdemo/gpdemo-env.sh

	cd "\${1}/gpdb_src/gpAux/extensions/pxf"
	make installcheck USE_PGXS=1

	[ -s regression.diffs ] && cat regression.diffs && exit 1

	export HADOOP_HOME=\${1}/singlecluster/hadoop
	cd "\${1}/gpdb_src/gpAux/extensions/pxf/regression/integrate"
	HADOOP_HOST=localhost HADOOP_PORT=8020 ./generate_hdfs_data.sh

	cd "\${1}/gpdb_src/gpAux/extensions/pxf/regression"
	GP_HADOOP_TARGET_VERSION=cdh4.1 HADOOP_HOST=localhost HADOOP_PORT=8020 ./run_pxf_regression.sh

	exit 0
	EOF

	chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
	chmod a+x /home/gpadmin/run_regression_test.sh

	su gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
}

function run_pxf_automation() {
	export GPHD_ROOT=$1
	pushd ${GPHD_ROOT} > /dev/null
		mkdir -p pxf && cd pxf
		ln -s ${PXF_HOME}/conf conf
		for X in ${PXF_HOME}/lib/pxf-*-[0-9]*.jar; do \
			ln -s ${X} $(echo $(basename ${X}) | sed -e 's/-[a-zA-Z0-9.]*.jar/.jar/'); \
		done
	popd > /dev/null

	cat > /home/gpadmin/run_pxf_automation_test.sh <<-EOF
	set -exo pipefail

	source ${GPHOME}/greenplum_path.sh
	source \${1}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh

	export PG_MODE=GPDB
	export GPHD_ROOT=\${1}/singlecluster

	# Copy PSI package from system python to GPDB as automation test requires it
	psi_dir=\$(find /usr/lib64 -name psi | sort -r | head -1)
	cp -r \${psi_dir} ${GPHOME}/lib/python
	psql -d template1 -c "CREATE EXTENSION PXF"
	cd \${1}/pxf_automation_src
	make GROUP=gpdb

	exit 0
	EOF

	chown gpadmin:gpadmin /home/gpadmin/run_pxf_automation_test.sh
	chmod a+x /home/gpadmin/run_pxf_automation_test.sh
	su gpadmin -c "bash /home/gpadmin/run_pxf_automation_test.sh $(pwd)"
}

function setup_gpadmin_user() {
	./gpdb_src/concourse/scripts/setup_gpadmin_user.bash "$TARGET_OS"
}

function unpack_tarball() {
	local tarball=$1
	echo "Unpacking tarball: $(ls ${tarball})"
	tar xfp ${tarball} --strip-components=1
}

function setup_singlecluster() {
	pushd singlecluster && if [ -f ./*.tar.gz ]; then \
		unpack_tarball ./*.tar.gz; \
	fi && popd

	pushd singlecluster/bin
	export SLAVES=1
	./init-gphd.sh
	./start-hdfs.sh
	popd
}

function start_pxf() {
	local hdfsrepo=$1
	pushd ${PXF_HOME} > /dev/null
	./bin/pxf init --hadoop-home ${hdfsrepo}/hadoop
	./bin/pxf start
	popd > /dev/null
}

function _main() {
	if [ -z "$TARGET_OS" ]; then
		echo "FATAL: TARGET_OS is not set"
		exit 1
	fi

	if [ "$TARGET_OS" != "centos" -a "$TARGET_OS" != "sles" ]; then
		echo "FATAL: TARGET_OS is set to an unsupported value: $TARGET_OS"
		echo "Configure TARGET_OS to be centos or sles"
		exit 1
	fi

	time configure
	time install_gpdb
	time setup_gpadmin_user
	time make_cluster

	time setup_singlecluster
	time start_pxf $(pwd)/singlecluster
	chown -R gpadmin:gpadmin $(pwd)
	time run_pxf_automation $(pwd)/singlecluster
	time run_regression_test
}

_main "$@"
