-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Lis 17, 2025 at 01:07 AM
-- Wersja serwera: 10.4.32-MariaDB
-- Wersja PHP: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `firma_pp`
--

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `dzialy`
--

CREATE TABLE `dzialy` (
  `id` int(11) NOT NULL,
  `nazwa` varchar(50) NOT NULL,
  `lokalizacja` varchar(50) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `dzialy`
--

INSERT INTO `dzialy` (`id`, `nazwa`, `lokalizacja`) VALUES
(1, 'Badania Wyładowań (R&D)', 'Warszawa'),
(2, 'Inżynieria Wysokich Napięć', 'Kraków'),
(3, 'Testy i Kontrola Jakości', 'Żyrardów'),
(4, 'Administracja i Finanse', 'Stare Babice'),
(5, 'Zasoby Ludzkie (HR)', 'Warszawa');

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `podwyzki`
--

CREATE TABLE `podwyzki` (
  `id` int(11) NOT NULL,
  `id_pracownika` int(11) NOT NULL,
  `kwota` decimal(10,2) NOT NULL,
  `data_wpisu` date NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `podwyzki`
--

INSERT INTO `podwyzki` (`id`, `id_pracownika`, `kwota`, `data_wpisu`) VALUES
(17, 27, 15000.00, '2022-01-01'),
(18, 28, 18000.00, '2024-03-01'),
(19, 30, 10500.00, '2023-07-01'),
(20, 26, 11500.00, '2024-11-01');

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `pracownicy`
--

CREATE TABLE `pracownicy` (
  `id` int(11) NOT NULL,
  `id_dzialu` int(11) NOT NULL,
  `login` varchar(100) NOT NULL,
  `haslo_hash` varchar(255) NOT NULL,
  `imie` varchar(50) NOT NULL,
  `nazwisko` varchar(50) NOT NULL,
  `wiek` int(11) DEFAULT NULL,
  `pozycja` varchar(100) NOT NULL,
  `placa_podstawa` decimal(10,2) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `pracownicy`
--

INSERT INTO `pracownicy` (`id`, `id_dzialu`, `login`, `haslo_hash`, `imie`, `nazwisko`, `wiek`, `pozycja`, `placa_podstawa`) VALUES
(26, 1, 'jakub.surma', '$2b$12$nqIeydP4G.dFAhJKfgUZueNIAwJhEIiapOYS.MiVc1j4BSK52J70C', 'Jakub', 'Surma', 32, 'Inżynier Badań', 11500.00),
(27, 2, 'albert.kolsut', '$2b$12$i5jrTSNvaRhL1a.DRgdmXuyrZCaP6x1XXMN64roUKfNq5rFYQm012', 'Albert', 'Kołsut', 28, 'Specjalista ds. Kontroli Napięć', 9800.00),
(28, 1, 'kajetan.wilkowski', '$2b$12$OTxVYNlEFrXS1Cxi6LgXWe5zGPhl.lhLho9vAP6VlY2kUwewoqhGq', 'Kajetan', 'Wilkowski', 45, 'Kierownik R&D', 18000.00),
(29, 4, 'ofiara.phishing', '$2b$12$5axeZverraBJEfJx98XiTexLiBKZjXEd.emdrgjhEsj7w2.6Z.7F.', 'Aneta', 'Nowak', 25, 'Specjalista ds. Subskrypcji', 6500.00),
(30, 3, 'marek.testowy', '$2b$12$4MhHS8Rnz5WQMCQskB4tOujXA8OHSe27QUKW9xCYtlLzcuira0.k2', 'Marek', 'Testowicz', 35, 'Technik Labolatoryjny', 7200.00);

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `pracownicy_projekty`
--

CREATE TABLE `pracownicy_projekty` (
  `id_pracownika` int(11) NOT NULL,
  `id_projektu` int(11) NOT NULL,
  `rola_w_projekcie` varchar(50) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `pracownicy_projekty`
--

INSERT INTO `pracownicy_projekty` (`id_pracownika`, `id_projektu`, `rola_w_projekcie`) VALUES
(26, 101, 'Główny Architekt'),
(27, 101, 'Analityk Energetyczny'),
(28, 104, 'Koordynator Zespołu'),
(29, 104, 'Asystent Techniczny'),
(30, 103, 'Weryfikacja Modułów');

-- --------------------------------------------------------

--
-- Struktura tabeli dla tabeli `projekty`
--

CREATE TABLE `projekty` (
  `id` int(11) NOT NULL,
  `nazwa` varchar(100) NOT NULL,
  `budzet` decimal(12,2) DEFAULT NULL,
  `status` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `projekty`
--

INSERT INTO `projekty` (`id`, `nazwa`, `budzet`, `status`) VALUES
(101, 'Symulator Burz Magnetycznych', 4500000.00, 'Aktywny'),
(102, 'Generator Błyskawic Typu C', 1200000.00, 'Wstrzymany'),
(103, 'System Ochrony Antypiorunowej W2', 800000.00, 'Zakończony'),
(104, 'Optymalizacja Energetyczna Źródła Z', 200000.00, 'Aktywny');

--
-- Indeksy dla zrzutów tabel
--

--
-- Indeksy dla tabeli `dzialy`
--
ALTER TABLE `dzialy`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `nazwa` (`nazwa`);

--
-- Indeksy dla tabeli `podwyzki`
--
ALTER TABLE `podwyzki`
  ADD PRIMARY KEY (`id`),
  ADD KEY `id_pracownika` (`id_pracownika`);

--
-- Indeksy dla tabeli `pracownicy`
--
ALTER TABLE `pracownicy`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `login` (`login`),
  ADD KEY `id_dzialu` (`id_dzialu`);

--
-- Indeksy dla tabeli `pracownicy_projekty`
--
ALTER TABLE `pracownicy_projekty`
  ADD PRIMARY KEY (`id_pracownika`,`id_projektu`),
  ADD KEY `id_projektu` (`id_projektu`);

--
-- Indeksy dla tabeli `projekty`
--
ALTER TABLE `projekty`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `nazwa` (`nazwa`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `dzialy`
--
ALTER TABLE `dzialy`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=6;

--
-- AUTO_INCREMENT for table `podwyzki`
--
ALTER TABLE `podwyzki`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=21;

--
-- AUTO_INCREMENT for table `pracownicy`
--
ALTER TABLE `pracownicy`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=42;

--
-- AUTO_INCREMENT for table `projekty`
--
ALTER TABLE `projekty`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=105;

--
-- Constraints for dumped tables
--

--
-- Constraints for table `podwyzki`
--
ALTER TABLE `podwyzki`
  ADD CONSTRAINT `podwyzki_ibfk_1` FOREIGN KEY (`id_pracownika`) REFERENCES `pracownicy` (`id`);

--
-- Constraints for table `pracownicy`
--
ALTER TABLE `pracownicy`
  ADD CONSTRAINT `pracownicy_ibfk_1` FOREIGN KEY (`id_dzialu`) REFERENCES `dzialy` (`id`);

--
-- Constraints for table `pracownicy_projekty`
--
ALTER TABLE `pracownicy_projekty`
  ADD CONSTRAINT `pracownicy_projekty_ibfk_1` FOREIGN KEY (`id_pracownika`) REFERENCES `pracownicy` (`id`) ON DELETE CASCADE,
  ADD CONSTRAINT `pracownicy_projekty_ibfk_2` FOREIGN KEY (`id_projektu`) REFERENCES `projekty` (`id`) ON DELETE CASCADE;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
