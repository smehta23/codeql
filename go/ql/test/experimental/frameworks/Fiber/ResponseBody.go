// Code generated by https://github.com/gagliardetto. DO NOT EDIT.

package main

import (
	"io"

	"github.com/gofiber/fiber"
)

// Package github.com/gofiber/fiber@v1.14.6
func ResponseBody_GithubComGofiberFiberV1146() {
	// Response body is set via a method call (the content-type is implicit in the method name).
	{
		// Response body is set via a method call on the github.com/gofiber/fiber.Ctx type (the content-type is implicit in the method name).
		{
			// func (*Ctx).JSON(data interface{}) error
			{
				bodyInterface768 := source().(interface{})
				var rece fiber.Ctx
				rece.JSON(bodyInterface768) // $ contentType=application/json responseBody=bodyInterface768
			}
			// func (*Ctx).JSONP(data interface{}, callback ...string) error
			{
				bodyInterface468 := source().(interface{})
				var rece fiber.Ctx
				rece.JSONP(bodyInterface468, "") // $ contentType=application/javascript responseBody=bodyInterface468
			}
		}
	}
	// Response body is set via a call of a type method.
	{
		// Response body is set via a call of a method on the github.com/gofiber/fiber.Ctx type.
		{
			// func (*Ctx).Format(body interface{})
			{
				bodyInterface736 := source().(interface{})
				var rece fiber.Ctx
				rece.Format(bodyInterface736) // $ responseBody=bodyInterface736
			}
			// func (*Ctx).Send(bodies ...interface{})
			{
				bodyInterface516 := source().(interface{})
				var rece fiber.Ctx
				rece.Send(bodyInterface516) // $ responseBody=bodyInterface516
			}
			// func (*Ctx).SendBytes(body []byte)
			{
				bodyByte246 := source().([]byte)
				var rece fiber.Ctx
				rece.SendBytes(bodyByte246) // $ responseBody=bodyByte246
			}
			// func (*Ctx).SendStream(stream io.Reader, size ...int)
			{
				bodyReader679 := source().(io.Reader)
				var rece fiber.Ctx
				rece.SendStream(bodyReader679, 0) // $ responseBody=bodyReader679
			}
			// func (*Ctx).SendString(body string)
			{
				bodyString736 := source().(string)
				var rece fiber.Ctx
				rece.SendString(bodyString736) // $ responseBody=bodyString736
			}
			// func (*Ctx).Write(bodies ...interface{})
			{
				bodyInterface839 := source().(interface{})
				var rece fiber.Ctx
				rece.Write(bodyInterface839) // $ responseBody=bodyInterface839
			}
		}
	}
}