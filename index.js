/* global exec */
'use strict'

let Pushover = false

require('shelljs/global')
const dgram = require('dgram')
const path = require('path')

const dateformat = require('dateformat')
const server = dgram.createSocket('udp4')
const signature = 'MT🚀'
const farSignature = 'GC📡'
const messageIcon = path.join(__dirname, 'media', 'Doorbell-76.png')
const messageLocalAudio = path.join(__dirname, 'media', 'Doorbell.caf')

const Notifier = require('node-notifier')
const nc = new Notifier.NotificationCenter()

if (process.env.pushover_user && process.env.pushover_token) {
	Pushover = require('pushover-notifications')
}

server.on('error', err => {
	console.log(`server error:\n${err.stack}`)
	server.close()
})

server.on('message', (msg, rinfo) => {
	console.log(`${msg} to ${signature} (${msg.length} bytes) from ${rinfo.address}:${rinfo.port}`)

	if ((rinfo.port === 8888 && msg.includes(farSignature)) || (rinfo.address === server.address().address && msg.includes(farSignature))) {
		const timestamp = Date.now()
		const msg = {
			title: 'Doorbell',
			message: `Pressed @${dateformat(timestamp, 'HH:MM:ss')}`,
			wait: false
		}
		if (Pushover) {
			msg.priority = 1
			new Pushover({
				user: process.env.pushover_user,
				token: process.env.pushover_token
			}).send(msg, (err_, result_) => {
				if (err_) {
					console.error(`Pushover Error: ${err_}`)
				} else {
					const result = JSON.parse(result_)
					console.log(`Pushover: ${result.request}`)
					if (result.status === 1) {
						setTimeout(() => {
							const message = new Buffer(`${signature}  +OK     `)
							console.log(`${message} to ${farSignature} ${rinfo.address}:${rinfo.port}`)
							server.send(message, 0, message.length, rinfo.port, rinfo.address, err => {
								if (err !== 0) {
									console.error(err)
								}
							})
						}, 5000)
					} else {
						const message = new Buffer(`${signature}  +BAD    `)
						console.log(`${message} to ${farSignature}`)
						server.send(message, 0, message.length, rinfo.port, rinfo.address, err => {
							if (err !== 0) {
								console.error(err)
							}
						})
					}
				}
			})
		}
		const ncMsg = msg
		ncMsg.wait = false
		ncMsg.icon = messageIcon
		nc.notify(ncMsg)
		exec(`afplay -v 1 ${messageLocalAudio} 2>/dev/null &`)
	}
})

server.on('listening', () => {
	const address = server.address()
	console.log(`server listening ${address.address}:${address.port}`)
})

server.bind(8989)
