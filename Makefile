all: tar

tar:
	mkdir move-package
	mkdir move-package/move
	cp -a move db tricks move-package/move
	make -C move-package/move/db fullclean
	make -C move-package/move/move fullclean
	make -C move-package/move/tricks fullclean
	(cd move-package && tar --exclude RCS -zcvf move.tar.gz move)
	rm -rf move-package/move
