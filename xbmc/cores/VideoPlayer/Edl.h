#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <string>
#include <vector>

class CEdl
{
public:
  CEdl();

  typedef enum
  {
    CUT = 0,
    MUTE = 1,
    // SCENE = 2,
    COMM_BREAK = 3
  } Action;

  struct Cut
  {
    int64_t start; // ms
    int64_t end;   // ms
    Action action;
  };

  bool ReadEditDecisionLists(const std::string& strMovie, const float fFramesPerSecond, const int iHeight);
  void Clear();

  bool HasCut() const;
  bool HasSceneMarker() const;
  std::string GetInfo() const;
  int64_t GetTotalCutTime() const;
  int64_t RemoveCutTime(int64_t iSeek) const;
  int64_t RestoreCutTime(int64_t dClock) const;

  bool InCut(int64_t iSeek, Cut *pCut = NULL);
  bool GetNearestCut(bool bPlus, const int64_t iSeek, Cut *pCut) const;

  int64_t GetLastCutTime() const;
  void SetLastCutTime(const int64_t iCutTime);

  bool GetNextSceneMarker(bool bPlus, const int64_t iClock, int64_t *iSceneMarker);

  static std::string MillisecondsToTimeString(const int64_t iMilliseconds);

private:
  int64_t m_iTotalCutTime; // ms
  std::vector<Cut> m_vecCuts;
  std::vector<int64_t> m_vecSceneMarkers;
  int64_t m_lastCutTime;

  bool ReadEdl(const std::string& strMovie, const float fFramesPerSecond);
  bool ReadComskip(const std::string& strMovie, const float fFramesPerSecond);
  bool ReadVideoReDo(const std::string& strMovie);
  bool ReadBeyondTV(const std::string& strMovie);
  bool ReadPvr(const std::string& strMovie);

  bool AddCut(Cut& NewCut);
  bool AddSceneMarker(const int64_t sceneMarker);

  void MergeShortCommBreaks();
};
