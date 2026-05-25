#include <iostream>
#include <vector>
#include <stdexcept>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <variant>
#include <optional>


struct RunArguments {
	std::optional<std::string> hostname;
	std::optional<std::string> domainname;
	std::vector<char*> cmd;
};

struct MayyaCommand {
	enum class Type { Run };
	Type type;
	std::variant<RunArguments> args;
};


struct ContainerConfig {
	std::string id;
	std::string hostname;
	std::string domainname;
	std::vector<char*> cmd;
};


constexpr auto usage = "Usage: ./mayya run [OPTIONS] <COMMAND> [ARG...]";

MayyaCommand parseArgs(int argc, char* argv[]) {
	if (argc < 3) {
		throw std::runtime_error("Wrong number of arguments");
	}
	MayyaCommand command;
	std::string argv1{argv[1]};
	if (argv1 == "run") {
		command.type = MayyaCommand::Type::Run;
		RunArguments runArgs;
		for (int i = 2; i < argc; ++i) {
			std::string arg{argv[i]};
			const std::string hostnameOpt = "--hostname=";
			const std::string domainnameOpt = "--domainname=";
			if (arg.find(hostnameOpt) != std::string::npos) {
				runArgs.hostname = arg.substr(hostnameOpt.size());
			} else if (arg.find(domainnameOpt) != std::string::npos) {
				runArgs.domainname = arg.substr(domainnameOpt.size());
			} else {
				runArgs.cmd.push_back(argv[i]);
			}
		}
		runArgs.cmd.push_back(nullptr);

		command.args = std::move(runArgs);
	} else {
		throw std::runtime_error("Invalid mayya command");
	}
	return command;
}


void* allocateStack() {
	constexpr std::size_t stackSize = 65'536;
	char *s = new char[stackSize];
	if (s == nullptr) {
		std::runtime_error("stack allocation failed");
	}
	return s + stackSize;
}


void check(int ret, const std::string& what) {
	if (ret == -1) {
		throw std::runtime_error(what + ": " + std::strerror(errno));
	}
}


std::string generateId(std::size_t lengthInBytes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < lengthInBytes; ++i) {
        ss << std::setw(2) << std::setfill('0') << dist(gen);
    }
    return  ss.str();
}


void setHostname(const std::string& hostname) {
	int ret = sethostname(hostname.c_str(), hostname.size());
	check(ret, "sethostname failed: ");
}


void setDomainname(const std::string& domainname) {
	int ret = setdomainname(domainname.c_str(), domainname.size());
	check(ret, "setdomainname failed: ");
}


int execChild(void *arg_) {
	int ret;
	try {
		ContainerConfig* config = static_cast<ContainerConfig*>(arg_);
		ret = execvp(config->cmd.at(0), const_cast<char* const*>(&config->cmd.data()[0]));
		check(ret, "execvp failed: ");
		ret = 0;
	} catch (const std::exception& e) {
		std::cerr << "execChild failed: " << e.what() << "\n";
		ret = -1;
	}
	return ret;
}


int setupChild(void *arg_) {
	int ret;
	try {
		ContainerConfig* config = static_cast<ContainerConfig*>(arg_);
		unshare(CLONE_NEWUTS);
		// Set hostname
		// TODO: write /etc/hostname, optionally generate /etc/hosts
		setHostname(config->hostname);
		if (!config->domainname.empty()) {
			setDomainname(config->domainname);
		}
		int cpid = clone(execChild, allocateStack(), SIGCHLD, arg_);
		check(cpid, "clone failed: ");
		int status;
		auto wpid = waitpid(cpid, &status, 0);
		check(cpid, "waitpid failed: ");
		ret = 0;
	} catch (const std::exception& e) {
		std::cerr << "setupChild failed: " << e.what() << "\n";
		ret = -1;
	}
	return ret;
}


void container_run(RunArguments args) {
	int ret;
	try {
		// Setup container config
		ContainerConfig* config = new ContainerConfig();
		std::string containerId = generateId(6);
		config->id = containerId;
		if (args.hostname.has_value() && !args.hostname.value().empty()) {
			config->hostname = args.hostname.value();
		} else {
			config->hostname = containerId;
		}
		if (args.domainname.has_value()) {
			config->domainname = args.domainname.value();
		}
		config->cmd = args.cmd;

		// Start setup child
		int cpid = clone(setupChild, allocateStack(), SIGCHLD, static_cast<void*>(config));
		check(cpid, "clone failed: ");
		int status;
		auto wpid = waitpid(cpid, &status, 0);
		check(cpid, "waitpid failed: ");
	} catch (const std::exception& e) {
		std::cerr << "setupChild failed: " << e.what() << "\n";
	}
}


int main(int argc, char* argv[]) {
	try {
		MayyaCommand command = parseArgs(argc, argv);
		if (command.type == MayyaCommand::Type::Run) {
			container_run(std::get<RunArguments>(command.args));
		}
	} catch (const std::exception& e) {
		std::cerr << e.what() << "\n\n";
		std::cerr << usage << "\n";
		return 1;
	}
	return 0;
}
