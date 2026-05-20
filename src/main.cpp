#include <iostream>
#include <vector>
#include <stdexcept>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>


struct Arguments {
	std::string mayyaCommand;
	std::vector<char*> cmd;
};

constexpr auto usage = "Usage: ./mayya run <cmd> [args]";

Arguments parseArgs(int argc, char* argv[]) {
	if (argc < 3) {
		throw std::runtime_error("Wrong number of arguments");
	}
	Arguments args;
	args.mayyaCommand = argv[1];
	if (args.mayyaCommand != "run") {
		throw std::runtime_error("Invalid mayya command");
	}
	args.cmd.assign(argv + 2, argv + argc);
	args.cmd.push_back(nullptr);
	return args;
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


int execChild(void *arg_) {
	int ret;
	try {
		std::vector<char*>* cmd = static_cast<std::vector<char*>*>(arg_);
		ret = execvp(cmd->at(0), const_cast<char* const*>(&cmd->data()[0]));
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


void container_run(std::vector<char*>& cmd) {
	int ret;
	try {
		int cpid = clone(setupChild, allocateStack(), SIGCHLD, static_cast<void*>(&cmd));
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
		Arguments args = parseArgs(argc, argv);
		if (args.mayyaCommand == "run") {
			container_run(args.cmd);
		}
	} catch (const std::exception& e) {
		std::cerr << e.what() << "\n\n";
		std::cerr << usage << "\n";
		return 1;
	}
	return 0;
}
