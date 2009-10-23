all:
	make -C move all
	make -C tricks all

clean:
	make -C move clean
	make -C tricks clean

tar:
	mkdir move-package
	mkdir move-package/move
	cp -Rp move db tricks move-package/move
	make -C move-package/move/db fullclean
	make -C move-package/move/move clean
	make -C move-package/move/tricks clean
	(cd move-package && tar --exclude RCS -zcvf move.tar.gz move)
	rm -rf move-package/move
