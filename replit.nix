{ pkgs }: {
	deps = [
   pkgs.cope
   pkgs.nano
		pkgs.clang_12
		pkgs.openssl
		pkgs.ccls
		pkgs.zlib
		pkgs.gdb
		pkgs.gcc
		pkgs.gnumake
	];
}