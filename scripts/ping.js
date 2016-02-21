'use strict'

const dgram = require('dgram')
const client = dgram.createSocket('udp4')

const farSignature = 'MT🚀'
const signature = 'GC📡'
const message = `${signature}    XXXXXXXX`
const port = process.env.npm_package_config_port || 8989
const broadcast = process.env.npm_package_config_broadcast || '255.255.255.255'

client.bind(8888, err => {
	if (err) {
		console.error(err)
		process.exit(1)
	}
	client.setBroadcast(true)

	client.on('error', err => {
		console.error(`network error:\n${err.stack}`)
		client.close()
	})

	client.on('message', (msg, rinfo) => {
		console.log(`${msg} to ${signature} (${msg.length} bytes) from ${rinfo.address}:${rinfo.port}`)
	})

	client.send(message, 0, message.length, port, broadcast, err => {
		console.log(`${message} to ${farSignature} (${message.length} bytes) to ${broadcast}:${port}`)
		if (err !== 0) {
			console.error(err)
		}
		client.close()
	})
})
