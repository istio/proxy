import {createContext} from 'react'
import {TAuthContext, TDataContext} from "./@types/app"

export const AuthContext = createContext<TAuthContext | null>(null)
export const DataContext = createContext<TDataContext | null>(null)
