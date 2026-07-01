#pragma once

#include <exception>

#include <stdexcept>

class IllegalMoveException : public std::exception {
public:
	explicit IllegalMoveException(const std::string& move, const std::string& message = "Illegal move!")
		: m_Move(move), m_Message(message + " " + move) {}

	const char* what() const noexcept override {
		return m_Message.c_str();
	}

	const std::string& move() const {
		return m_Move;
	}
private:
	std::string m_Move;
	std::string m_Message;
};

class InvalidAlgebraicMoveException : public std::exception {
public:
	InvalidAlgebraicMoveException(const std::string& move) : m_Move(move) {}
	InvalidAlgebraicMoveException(std::string_view move) : m_Move(move) {}

	const char* what() const noexcept override {
		return "Invalid algebraic notation!";
	}

	const std::string& move() const {
		return m_Move;
	}
private:
	std::string m_Move;
};

class InvalidLongAlgebraicMoveException : public std::exception {
public:
	explicit InvalidLongAlgebraicMoveException(const std::string& move) : m_Move(move) {}

	const char* what() const noexcept override {
		return "Invalid long algebraic notation!";
	}

	const std::string& move() const {
		return m_Move;
	}
private:
	std::string m_Move;
};

// Thrown only by chess::notation::ParseLongAlgebraicNotation when a move string
// is not parseable as Long Algebraic Notation. This is purely a *notational*
// failure, kept distinct from game-level claim errors (e.g. claiming check
// without giving check), which are not exceptions from the parser.
class MalformedLongAlgebraicNotationException : public std::exception {
public:
	explicit MalformedLongAlgebraicNotationException(const std::string& move) : m_Move(move) {}

	const char* what() const noexcept override {
		return "Malformed long algebraic notation!";
	}

	const std::string& move() const {
		return m_Move;
	}
private:
	std::string m_Move;
};

class InvalidCaptureException : public std::exception {
public:
	explicit InvalidCaptureException(std::string move) : m_Move(std::move(move)) {}

	const char* what() const noexcept override {
		return "Capture notation used but no piece to capture";
	}

	const std::string& move() const {
		return m_Move;
	}
private:
	std::string m_Move;
};

class InvalidFenException : public std::exception {
public:
	InvalidFenException() : m_Message("Invalid FEN string!") {}
	explicit InvalidFenException(const std::string& message) : m_Message(message) {}

	const char* what() const noexcept override {
		return m_Message.c_str();
	}
private:
	std::string m_Message;
};

class InvalidPieceTypeException : public std::exception {
public:
	const char* what() const noexcept override {
		return "Invalid piece type!";
	}
};

class InvalidPgnException : public std::exception {
public:
	explicit InvalidPgnException(const std::string& message) : m_Message(message) {}

	const char* what() const noexcept override {
		return m_Message.c_str();
	}
private:
	std::string m_Message;
};

class SeekOutOfBoundsException : public std::exception {
public:
	const char* what() const noexcept override {
		return "Attempted to Seek() out of bounds!";
	}
};

class DeleteOutOfBoundsException : public std::exception {
public:
	const char* what() const noexcept override {
		return "Attempted to Delete() out of bounds!";
	}
};
