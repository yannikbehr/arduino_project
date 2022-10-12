#!/usr/bin/env Rscript

dateStart = as.Date('2022-10-02')
dateEnd = Sys.Date()

data = character()
while (dateStart<=dateEnd){

	month = substr(format(dateStart, '%B'), 1, 3)
	fname=paste("data/", format(dateStart, '%d_'), month, sep="")

	readDat=T
	if (dateStart < dateEnd && file.exists(fname)){
		dat <- readRDS(fname, refhook = NULL)
		readDat=F
	}

	if (readDat){
		cat(format(dateStart,'%A %B %d, %Y\n'))
		dat = system(paste("./query_by_date.sh ", format(dateStart, '%d/'), month, sep=""), intern=T)
		cat(paste(dat[length(dat)], "\n", sep=""))
	}

	if (dateStart < dateEnd ){
		saveRDS(dat, file = fname, ascii = FALSE, version = NULL,compress = TRUE, refhook = NULL)
	}
	
	data = c(data, dat)
	dateStart = dateStart+1
}

datF = as.data.frame(strsplit(data, "\t"), stringsAsFactors=F)

dd = lapply(datF[2, ], function(d){strptime(strsplit(strsplit(d, "_")[[1]][2], " ")[[1]][1],format='%d/%B/%Y:%H:%M:%S')})
mydates <- do.call("c", dd)


df = data.frame(times = mydates, val=as.numeric(datF[1, ]), stringsAsFactors=F, row.names=NULL)

idx = mydates > strptime("03/June/2021:06:7:38",format='%d/%B/%Y:%H:%M:%S') & datF[1, ] != 1000
#idx = mydates > strptime("20/June/2021:06:7:38",format='%d/%B/%Y:%H:%M:%S') & datF[1, ] != 1000
mydates = mydates + 2 * 3600 # times seem to be two hours off for som reason

df = df[idx, ]

library(ggplot2)

pdf("GewichtsKurve.pdf")
ggplot(data=df, aes(x=times, y=val))+geom_line() + geom_smooth() + ylab("Gewicht [g]") + xlab("Zeitpunkt")
#ggplot(data=df, aes(x=dates, y=values))+geom_line()
#p = ggplot(data=df, aes(x=dates, y=values))+geom_point(size=1.5)
#p = p + geom_line()
#p = p + scale_x_date(date_breaks = '3 day', date_labels = '%b %d')+
#p
#plot(mydates[idx], datF[1, idx], type="l", xlab="Zeit", ylab="Gewicht [g]")
dev.off()
