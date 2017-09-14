#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "${CWDIR}/common.bash"

function gen_env(){
	cat > /home/gpadmin/run_regression_test.sh <<-EOF
	set -exo pipefail

	source /opt/gcc_env.sh
	source /usr/local/greenplum-db-devel/greenplum_path.sh

	cd "\${1}/gpdb_src/gpAux"
	source gpdemo/gpdemo-env.sh

	if [ "$overwrite_pxf" = "true" ]
	then
		cd "\${1}/gpdb_src/gpAux/extensions/pxf"
		make install
	fi

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

	chown -R gpadmin:gpadmin $(pwd)
	chown gpadmin:gpadmin /home/gpadmin/run_regression_test.sh
	chmod a+x /home/gpadmin/run_regression_test.sh
}

function run_regression_test() {
	su gpadmin -c "bash /home/gpadmin/run_regression_test.sh $(pwd)"
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
	local pxfhome="/usr/local/greenplum-db-devel/pxf"
	pushd ${pxfhome} > /dev/null
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
	time gen_env

	time setup_singlecluster
	time start_pxf $(pwd)/singlecluster
	time run_regression_test
}

_main "$@"
