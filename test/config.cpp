/****
 *
 * This file is auto-generated.
 *
 ****/

#include "config.h"

using namespace ConfigDB;

std::unique_ptr<Object> Config::RootStore::getObject(unsigned index)
{
	switch(index) {
	case 0:
		return std::make_unique<Config::General>(getPointer());
	case 1:
		return std::make_unique<Config::Security>(getPointer());
	case 2:
		return std::make_unique<Config::Color>(getPointer());
	case 3:
		return std::make_unique<Config::Color::Brightness>(getPointer());
	case 4:
		return std::make_unique<Config::Color::Colortemp>(getPointer());
	case 5:
		return std::make_unique<Config::Color::Hsv>(getPointer());
	case 6:
		return std::make_unique<Config::Ota>(getPointer());
	case 7:
		return std::make_unique<Config::Sync>(getPointer());
	case 8:
		return std::make_unique<Config::Events>(getPointer());
	case 9:
		return std::make_unique<Config::Network>(getPointer());
	case 10:
		return std::make_unique<Config::Network::Mqtt>(getPointer());
	case 11:
		return std::make_unique<Config::Network::Connection>(getPointer());
	case 12:
		return std::make_unique<Config::Network::Ap>(getPointer());
	default:
		return nullptr;
	}
}

std::unique_ptr<Object> Config::General::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::General::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_devicename, Property::Type::String, nullptr};
	case 1:
		return {*this, fstr_pinconfigu, Property::Type::String, nullptr};
	case 3:
		return {*this, fstr_currentpin, Property::Type::String, nullptr};
	case 4:
		return {*this, fstr_buttonsdeb, Property::Type::Integer, nullptr};
	case 6:
		return {*this, fstr_pinconfig, Property::Type::String, nullptr};
	case 7:
		return {*this, fstr_buttonscon, Property::Type::String, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Security::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Security::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_apisecured, Property::Type::Boolean, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Color::Brightness::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Color::Brightness::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_red, Property::Type::Integer, nullptr};
	case 1:
		return {*this, fstr_ww, Property::Type::Integer, nullptr};
	case 2:
		return {*this, fstr_green, Property::Type::Integer, nullptr};
	case 3:
		return {*this, fstr_blue, Property::Type::Integer, nullptr};
	case 4:
		return {*this, fstr_cw, Property::Type::Integer, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Color::Colortemp::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Color::Colortemp::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_ww, Property::Type::Integer, nullptr};
	case 1:
		return {*this, fstr_cw, Property::Type::Integer, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Color::Hsv::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Color::Hsv::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_red, Property::Type::Integer, nullptr};
	case 1:
		return {*this, fstr_magenta, Property::Type::Integer, nullptr};
	case 2:
		return {*this, fstr_green, Property::Type::Integer, nullptr};
	case 3:
		return {*this, fstr_blue, Property::Type::Integer, nullptr};
	case 4:
		return {*this, fstr_yellow, Property::Type::Integer, nullptr};
	case 5:
		return {*this, fstr_model, Property::Type::Integer, nullptr};
	case 6:
		return {*this, fstr_cyan, Property::Type::Integer, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Color::getObject(unsigned index)
{
	switch(index) {
	case 0:
		return std::make_unique<Config::Color::Brightness>(store);
	case 1:
		return std::make_unique<Config::Color::Colortemp>(store);
	case 2:
		return std::make_unique<Config::Color::Hsv>(store);
	default:
		return nullptr;
	}
}

Property Config::Color::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_startupcol, Property::Type::String, nullptr};
	case 3:
		return {*this, fstr_outputmode, Property::Type::Integer, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Ota::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Ota::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_url, Property::Type::String, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Sync::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Sync::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_cmdmastere, Property::Type::Boolean, nullptr};
	case 1:
		return {*this, fstr_colorslave, Property::Type::Boolean, nullptr};
	case 2:
		return {*this, fstr_colorslave_0, Property::Type::String, nullptr};
	case 3:
		return {*this, fstr_clockmaste, Property::Type::Boolean, nullptr};
	case 4:
		return {*this, fstr_colormaste, Property::Type::Integer, nullptr};
	case 5:
		return {*this, fstr_clockslave, Property::Type::Boolean, nullptr};
	case 6:
		return {*this, fstr_cmdslaveen, Property::Type::Boolean, nullptr};
	case 7:
		return {*this, fstr_clockmaste_0, Property::Type::Integer, nullptr};
	case 8:
		return {*this, fstr_clockslave_0, Property::Type::String, nullptr};
	case 9:
		return {*this, fstr_cmdslaveto, Property::Type::String, nullptr};
	case 10:
		return {*this, fstr_colormaste_0, Property::Type::Boolean, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Events::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Events::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_colorinter, Property::Type::Integer, nullptr};
	case 1:
		return {*this, fstr_colorminin, Property::Type::Integer, nullptr};
	case 2:
		return {*this, fstr_transfinin, Property::Type::Integer, nullptr};
	case 3:
		return {*this, fstr_serverenab, Property::Type::Boolean, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Network::Mqtt::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Network::Mqtt::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_server, Property::Type::String, nullptr};
	case 1:
		return {*this, fstr_password, Property::Type::String, nullptr};
	case 2:
		return {*this, fstr_port, Property::Type::Integer, nullptr};
	case 3:
		return {*this, fstr_topicbase, Property::Type::String, nullptr};
	case 4:
		return {*this, fstr_enabled, Property::Type::Boolean, nullptr};
	case 5:
		return {*this, fstr_username, Property::Type::String, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Network::Connection::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Network::Connection::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_netmask, Property::Type::String, nullptr};
	case 1:
		return {*this, fstr_ip, Property::Type::String, nullptr};
	case 2:
		return {*this, fstr_dhcp, Property::Type::Boolean, nullptr};
	case 3:
		return {*this, fstr_gateway, Property::Type::String, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Network::Ap::getObject(unsigned index)
{
	switch(index) {
	default:
		return nullptr;
	}
}

Property Config::Network::Ap::getProperty(unsigned index)
{
	switch(index) {
	case 0:
		return {*this, fstr_password, Property::Type::String, nullptr};
	case 1:
		return {*this, fstr_secured, Property::Type::Boolean, nullptr};
	case 2:
		return {*this, fstr_ssid, Property::Type::String, nullptr};
	default:
		return {*this};
	}
}

std::unique_ptr<Object> Config::Network::getObject(unsigned index)
{
	switch(index) {
	case 0:
		return std::make_unique<Config::Network::Mqtt>(store);
	case 1:
		return std::make_unique<Config::Network::Connection>(store);
	case 2:
		return std::make_unique<Config::Network::Ap>(store);
	default:
		return nullptr;
	}
}

Property Config::Network::getProperty(unsigned index)
{
	switch(index) {
	default:
		return {*this};
	}
}

std::shared_ptr<Store> Config::getStore(unsigned index)
{
	switch(index) {
	case 0:
		return RootStore::open(*this);
	default:
		return nullptr;
	}
}
